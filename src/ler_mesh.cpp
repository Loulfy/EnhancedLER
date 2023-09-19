//
// Created by loulfy on 30/08/2023.
//

#include "ler_mesh.hpp"

#include <utility>

namespace ler
{
    void SceneBuffers::allocate(const LerDevicePtr& device)
    {
        m_indexBuffer = device->createBuffer(kMaxBufferSize, vk::BufferUsageFlagBits::eIndexBuffer);
        for(BufferPtr& buf : m_vertexBuffers)
            buf = device->createBuffer(kMaxBufferSize, vk::BufferUsageFlagBits::eVertexBuffer);
        for(BufferPtr& buf : m_staticBuffers)
            buf = device->createBuffer(kMaxBufferSize, vk::BufferUsageFlagBits::eStorageBuffer);
    }

    void SceneBuffers::bind(const CommandPtr& cmd, bool prePass) const
    {
        constexpr static vk::DeviceSize offset = 0;
        size_t n = prePass ? 1 : m_vertexBuffers.size();
        cmd->cmdBuf.bindIndexBuffer(m_indexBuffer->handle, offset, vk::IndexType::eUint32);
        for(size_t i = 0; i < n; ++i)
            cmd->cmdBuf.bindVertexBuffers(i, 1, &m_vertexBuffers[i]->handle, &offset);
    }

    const BufferPtr& SceneBuffers::getMaterialBuffer() const
    {
        return m_staticBuffers[0];
    }

    const BufferPtr& SceneBuffers::getMeshletBuffer() const
    {
        return m_staticBuffers[1];
    }

    IndexedMesh SceneBuffers::getMeshInfo(uint32_t meshId) const
    {
        if(meshId < kMaxMesh)
            return meshes[meshId];
        return nullptr;
    }

    std::vector<SceneImporter::SceneSubmissionPtr> SceneImporter::m_submissions;

    bool SceneImporter::LoadScene(const LerDevicePtr& device, SceneBuffers& scene, FsTag tag, const fs::path& path)
    {
        auto ext = FileSystemService::Get(tag)->format_hint(path);
        std::string list;
        static Assimp::Importer importer;
        importer.GetExtensionList(list);
        if(list.find(ext.string()) == std::string::npos)
            return false;

        SceneSubmissionPtr& submission = m_submissions.emplace_back(std::make_shared<SceneSubmission>());
        submission->scene = &scene;
        Async::GetPool().push_task(processMeshes, device, tag, path, submission);
        return true;
    }

    std::vector<SceneBuffers*> SceneImporter::PollUpdate(const LerDevicePtr& device, flecs::world& world)
    {
        std::vector<SceneBuffers*> result;
        TexturePoolPtr pool = device->getTexturePool();
        auto it = m_submissions.begin();
        while(it < m_submissions.end())
        {
            SceneSubmissionPtr sub = *it;
            if(device->pollCommand(sub->submissionId) && pool->verify(sub->dependency))
            {
                it = m_submissions.erase(it);
                processSceneGraph(sub, world);
                result.emplace_back(sub->scene);
            }
            else
            {
                ++it;
            }

        }
        return result;
    }

    void populateBufferCopy(std::byte* dest, vk::BufferCopy& bufferCopy, const aiScene* aiScene,
                            const std::function<bool(aiMesh*)>& predicate,
                            const std::function<void*(aiMesh*)>& provider)
    {
        size_t byteSize;
        bufferCopy.size = 0;
        for (unsigned int i = 0; i < aiScene->mNumMeshes; ++i)
        {
            aiMesh* mesh = aiScene->mMeshes[i];
            byteSize = mesh->mNumVertices * sizeof(glm::vec3);
            if (predicate(mesh))
                std::memcpy(dest + bufferCopy.srcOffset + bufferCopy.size, provider(mesh), byteSize);
            bufferCopy.size += byteSize;
        }
    }

    void SceneImporter::processMeshes(const LerDevicePtr& device, FsTag tag, const fs::path& path, const SceneSubmissionPtr& submission)
    {
        unsigned int postProcess = aiProcessPreset_TargetRealtime_Fast;
        postProcess |= aiProcess_ConvertToLeftHanded;
        postProcess |= aiProcess_GenBoundingBoxes;
        const aiScene* aiScene;
        if (tag == FsTag_Default)
        {
            aiScene = submission->importer.ReadFile(path.string(), postProcess);
        }
        else
        {
            const auto blob = FileSystemService::Get().readFile(tag, path);
            aiScene = submission->importer.ReadFileFromMemory(blob.data(), blob.size(), postProcess, path.string().c_str());
        }

        if (aiScene == nullptr || aiScene->mNumMeshes < 0)
        {
            log::error(submission->importer.GetErrorString());
            return;
        }

        if (aiScene->mNumTextures == 0)
            FileSystemService::Get().mount(FsTag_Assimp, StdFileSystem::Create(ASSETS_DIR)); //StdFileSystem::Create(path.parent_path()));
        else
            FileSystemService::Get().mount(FsTag_Assimp, AssimpFileSystem::Create(aiScene));

        SceneBuffers* scene = submission->scene;

        // Register Meshes
        uint32_t meshOffset = scene->meshCount.fetch_add(aiScene->mNumMeshes);

        uint32_t indexCount = 0;
        uint32_t vertexCount = 0;
        for (size_t i = 0; i < aiScene->mNumMeshes; ++i)
        {
            aiMesh* mesh = aiScene->mMeshes[i];
            scene->meshes[i + meshOffset] = std::make_shared<MeshInfo>();
            MeshInfo* info = scene->meshes[i + meshOffset].get();
            info->countIndex = mesh->mNumFaces * 3;
            info->firstIndex = indexCount;
            info->countVertex = mesh->mNumVertices;
            info->firstVertex = static_cast<int32_t>(vertexCount);
            info->bMin = glm::make_vec3(&mesh->mAABB.mMin[0]);
            info->bMax = glm::make_vec3(&mesh->mAABB.mMax[0]);
            info->materialId = mesh->mMaterialIndex;
            info->name = mesh->mName.C_Str();

            indexCount += info->countIndex;
            vertexCount += info->countVertex;

            std::vector<uint32_t> indices;
            indices.reserve(mesh->mNumFaces*3);
            for (size_t j = 0; j < mesh->mNumFaces; ++j)
                indices.insert(indices.end(), mesh->mFaces[j].mIndices, mesh->mFaces[j].mIndices + 3);

            float threshold = 0.1f;
            uint32_t index_count = indices.size();
            size_t target_index_count = size_t(index_count * threshold);
            float target_error = 1e-2f;

            std::vector<uint32_t> lod(index_count);
            float lod_error = 0.f;
            lod.resize(meshopt_simplifySloppy(&lod[0], indices.data(), index_count, &mesh->mVertices[0].x, mesh->mNumVertices, sizeof(aiVector3D),
                                        target_index_count, target_error, &lod_error));

            //std::vector<aiVector3D> vert(mesh->mNumVertices);
            //vert.resize(meshopt_optimizeVertexFetch(&vert[0], lod.data(), lod.size(), &mesh->mVertices[0].x, mesh->mNumVertices, sizeof(aiVector3D)));

            physx::PxTriangleMeshDesc meshDesc;
            meshDesc.points.count           = mesh->mNumVertices;
            meshDesc.points.stride          = sizeof(physx::PxVec3);
            meshDesc.points.data            = mesh->mVertices;

            meshDesc.triangles.count        = lod.size()/3; //mesh->mNumFaces;
            meshDesc.triangles.stride       = 3*sizeof(physx::PxU32);
            meshDesc.triangles.data         = lod.data(); //indices.data();

            physx::PxConvexMeshDesc convexDesc;
            convexDesc.points.count     = mesh->mNumVertices;
            convexDesc.points.stride    = sizeof(physx::PxVec3);
            convexDesc.points.data      = mesh->mVertices;
            convexDesc.flags            = physx::PxConvexFlag::eCOMPUTE_CONVEX | physx::PxConvexFlag::eDISABLE_MESH_VALIDATION | physx::PxConvexFlag::eFAST_INERTIA_COMPUTATION;

            //ind->collision = PXInitializer::BuildConvexMesh(convexDesc);
            info->collision = PXInitializer::BuildTriangleMesh(meshDesc);
            info->bounds = calculateMeshBounds(mesh);
        }

        uint32_t indexDstOffset = scene->indexCount.fetch_add(indexCount);
        uint32_t vertexDstOffset = scene->vertexCount.fetch_add(vertexCount);

        // Register Indirect Mesh (GPU Side)
        std::vector<Meshlet> meshes(aiScene->mNumMeshes);
        for (size_t i = 0; i < aiScene->mNumMeshes; ++i)
        {
            MeshInfo* info = scene->meshes[i + meshOffset].get();
            info->firstIndex += indexDstOffset;
            info->firstVertex += static_cast<int32_t>(vertexDstOffset);

            meshes[i].countIndex = info->countIndex;
            meshes[i].firstIndex = info->firstIndex;
            meshes[i].vertexOffset = info->firstVertex;
        }

        // Register Material
        aiString filename;
        aiColor3D baseColor;
        TexturePool::Require key;
        TexturePoolPtr pool = device->getTexturePool();
        std::vector<Material> materials(aiScene->mNumMaterials);
        for (size_t i = 0; i < aiScene->mNumMaterials; ++i)
        {
            aiMaterial* material = aiScene->mMaterials[i];
            material->Get(AI_MATKEY_COLOR_DIFFUSE, baseColor);
            materials[i].color = glm::vec3(baseColor.r, baseColor.g, baseColor.b);
            if (material->GetTextureCount(aiTextureType_BASE_COLOR) > 0)
            {
                material->GetTexture(aiTextureType_BASE_COLOR, 0, &filename);
                materials[i].texId = pool->fetch(filename.C_Str(), FsTag_Assimp);
                key.set(materials[i].texId);
            }
            if (material->GetTextureCount(aiTextureType_AMBIENT) > 0)
            {
                material->GetTexture(aiTextureType_AMBIENT, 0, &filename);
                materials[i].texId = pool->fetch(filename.C_Str(), FsTag_Assimp);
                key.set(materials[i].texId);
            }
            if (material->GetTextureCount(aiTextureType_DIFFUSE) > 0)
            {
                material->GetTexture(aiTextureType_DIFFUSE, 0, &filename);
                materials[i].texId = pool->fetch(filename.C_Str(), FsTag_Assimp);
                key.set(materials[i].texId);
            }
            if (material->GetTextureCount(aiTextureType_NORMALS) > 0)
            {
                material->GetTexture(aiTextureType_NORMALS, 0, &filename);
                materials[i].norId = pool->fetch(filename.C_Str(), FsTag_Assimp);
                key.set(materials[i].norId);
            }
        }

        submission->dependency = key;

        uint32_t materialOffset = scene->materialCount.fetch_add(aiScene->mNumMaterials);

        // Merge index
        std::vector<uint32_t> indices;
        indices.reserve(indexCount);
        for (size_t i = 0; i < aiScene->mNumMeshes; ++i)
        {
            aiMesh* mesh = aiScene->mMeshes[i];
            if (mesh->HasFaces())
            {
                for (size_t j = 0; j < mesh->mNumFaces; ++j)
                {
                    indices.insert(indices.end(), mesh->mFaces[j].mIndices, mesh->mFaces[j].mIndices + 3);
                }
            }
        }

        // Allocate staging buffer
        BufferPtr staging = device->createBuffer(SceneBuffers::kMaxBufferSize*6, vk::BufferUsageFlagBits(), true);
        void* data = staging->hostInfo.pMappedData;
        auto* dest = static_cast<std::byte*>(data);

        indexDstOffset *= sizeof(uint32_t);
        vertexDstOffset *= sizeof(glm::vec3);

        // Prepare copy commands
        std::array<vk::BufferCopy, 7> copies;
        copies[0].size = indices.size() * sizeof(uint32_t);

        // 0 : Index
        size_t offset = 0;
        copies[0].srcOffset = offset;
        copies[0].dstOffset = indexDstOffset;
        std::memcpy(dest, indices.data(), copies[0].size);

        // 1 : Vertex
        offset += copies[0].size;
        copies[1].srcOffset = offset;
        copies[1].dstOffset = vertexDstOffset;
        populateBufferCopy(dest, copies[1], aiScene, [](aiMesh* mesh)
        { return mesh->HasPositions(); }, [](aiMesh* mesh)
                           { return mesh->mVertices; });

        // 2 : Normal
        offset += copies[1].size;
        copies[2].srcOffset = offset;
        copies[2].dstOffset = vertexDstOffset;
        populateBufferCopy(dest, copies[2], aiScene, [](aiMesh* mesh)
        { return mesh->HasNormals(); }, [](aiMesh* mesh)
                           { return mesh->mNormals; });

        // 3 : TexCoord
        offset += copies[2].size;
        copies[3].srcOffset = offset;
        copies[3].dstOffset = vertexDstOffset;
        populateBufferCopy(dest, copies[3], aiScene, [](aiMesh* mesh)
        { return mesh->HasTextureCoords(0); }, [](aiMesh* mesh)
                           { return mesh->mTextureCoords[0]; });

        // 4 : Tangent
        offset += copies[3].size;
        copies[4].srcOffset = offset;
        copies[4].dstOffset = vertexDstOffset;
        populateBufferCopy(dest, copies[4], aiScene, [](aiMesh* mesh)
        { return mesh->HasTangentsAndBitangents(); }, [](aiMesh* mesh)
                           { return mesh->mTangents; });

        // 5 : Material
        offset += copies[4].size;
        copies[5].srcOffset = offset;
        copies[5].dstOffset = materialOffset*sizeof(Material);
        copies[5].size = materials.size()*sizeof(Material);
        std::memcpy(dest + copies[5].srcOffset, materials.data(), copies[5].size);

        // 6 : Mesh
        offset += copies[5].size;
        copies[6].srcOffset = offset;
        copies[6].dstOffset = meshOffset*sizeof(Meshlet);
        copies[6].size = meshes.size()*sizeof(Meshlet);
        std::memcpy(dest + copies[6].srcOffset, meshes.data(), copies[6].size);

        SubmitScene sub;
        sub.submission = submission;
        sub.command = device->createCommand(CommandQueue::Transfer);
        sub.command->copyBuffer(staging, scene->m_indexBuffer, copies[0]);
        /*for(size_t i = 0; i < scene->m_vertexBuffers.size(); ++i)
            sub.command->copyBuffer(staging, scene->m_vertexBuffers[i], copies[i + 1]);*/

        sub.command->copyBuffer(staging, scene->m_vertexBuffers[0], copies[1]);
        sub.command->copyBuffer(staging, scene->m_vertexBuffers[1], copies[3]);
        sub.command->copyBuffer(staging, scene->m_vertexBuffers[2], copies[2]);
        sub.command->copyBuffer(staging, scene->m_vertexBuffers[3], copies[4]);
        for(size_t i = 0; i < scene->m_staticBuffers.size(); ++i)
            sub.command->copyBuffer(staging, scene->m_staticBuffers[i], copies[i + 5]);

        AsyncQueue<AsyncRequest>::Commit(sub);
    }

    void SceneImporter::processSceneGraph(const SceneSubmissionPtr& submission, flecs::world& world)
    {
        const aiScene* aiScene = submission->importer.GetScene();
        processSceneNode(world, aiScene->mRootNode, 0, submission->scene);
    }

    void SceneImporter::processSceneNode(flecs::world& world, aiNode* aiNode, uint32_t meshOffset, SceneBuffers* scene)
    {
        if(aiNode == nullptr)
            return;

        for (size_t i = 0; i < aiNode->mNumMeshes; ++i)
        {
            uint32_t meshId = aiNode->mMeshes[i]+meshOffset;
            auto& ind = scene->meshes[meshId];
            auto node = world.entity();

            aiMatrix4x4 model = aiNode->mTransformation;
            auto* currentParent = aiNode->mParent;
            while (currentParent) {
                model = currentParent->mTransformation * model;
                currentParent = currentParent->mParent;
            }

            PhysicBuilder builder(model);
            const auto t = glm::make_mat4(model.Transpose()[0]);

            glm::vec3 min = ind->bMin;
            glm::vec3 max = ind->bMax;

            node.set<CMesh>({meshId, min, max, ind->bounds});
            node.set<CTransform>({t});
            node.set<CMaterial>({ind->materialId});

            min = glm::vec3(t * glm::vec4(min, 1.f));
            max = glm::vec3(t * glm::vec4(max, 1.f));

            std::visit(builder, ind->collision);
            node.set<CPhysic>({builder.actor});
            auto tag = new UserData;
            tag->e = node;
            builder.actor->userData = static_cast<void*>(tag);
        }

        for(size_t i = 0; i < aiNode->mNumChildren; ++i)
            processSceneNode(world, aiNode->mChildren[i], meshOffset, scene);
    }

    physx::PxMeshScale PhysicBuilder::create()
    {
        physx::PxTransform pxt;
        physx::PxMeshScale scale;
        //auto dd = _model;
        //model = glm::make_mat4(_model.Transpose()[0]);
        convertToPx(m_model, pxt, scale);
        actor = PxGetPhysics().createRigidStatic(pxt);
        return scale;
    }

    void PhysicBuilder::operator()(physx::PxConvexMesh* mesh)
    {
        auto scale = create();
        physx::PxMaterial* material = PxGetPhysics().createMaterial(0.5f, 0.5f, 0.6f);
        physx::PxConvexMeshGeometry geometry(mesh, scale);
        physx::PxRigidActorExt::createExclusiveShape(*actor, geometry, *material);
    }
    void PhysicBuilder::operator()(physx::PxTriangleMesh* mesh)
    {
        auto scale = create();
        physx::PxMaterial* material = PxGetPhysics().createMaterial(0.5f, 0.5f, 0.6f);
        physx::PxTriangleMeshGeometry geometry(mesh, scale);
        physx::PxRigidActorExt::createExclusiveShape(*actor, geometry, *material);
    }

    void PhysicBuilder::operator()(std::monostate)
    {
        /*
        glm::vec3 min = indexedMesh->bMin;
        glm::vec3 max = indexedMesh->bMax;
        glm::vec3 ss(scale.scale.x, scale.scale.y, scale.scale.z);
        min*= ss;
        max*= ss;
        glm::vec3 box = max - min;

        min = indexedMesh->bMin;
        max = indexedMesh->bMax;
        min = glm::vec3(model * glm::vec4(min, 1.f));
        max = glm::vec3(model * glm::vec4(max, 1.f));
        glm::vec3 c = (min+max)/2.f;
        physx::PxTransform pxt(physx::PxVec3(c.x, c.y, c.z));
        actor = PxGetPhysics().createRigidStatic(pxt);


        auto [actor, scale] = create();
        physx::PxMaterial* material = PxGetPhysics().createMaterial(0.5f, 0.5f, 0.6f);
        physx::PxBoxGeometry geometry(box.x, box.y, box.z);
        if(!geometry.isValid())
            log::info("lol");
        physx::PxRigidActorExt::createExclusiveShape(*actor, geometry, *material);*/
    }

    struct RawVertex
    {
        f32 position[3];
        f32 normal[3];
        f32 texCoord[2];
    };

    void SceneImporter::testy(aiMesh* mesh)
    {
        size_t index_count = mesh->mNumFaces*3;
        std::vector<RawVertex> vertices;
        vertices.reserve(index_count);

        std::vector<u32> remapTable(index_count);
        size_t vertexCount = meshopt_generateVertexRemap(remapTable.data(), nullptr, index_count,
                                                         vertices.data(), vertices.size(), sizeof(RawVertex));

        vertices.resize(vertexCount);
        std::vector<u32> indices(index_count);

        meshopt_remapVertexBuffer(vertices.data(), vertices.data(), indices.size(), sizeof(RawVertex), remapTable.data());
        meshopt_remapIndexBuffer(indices.data(), nullptr, indices.size(), remapTable.data());

        meshopt_optimizeVertexCache(indices.data(), indices.data(), indices.size(), vertices.size());
        meshopt_optimizeOverdraw(indices.data(), indices.data(), indices.size(), &vertices[0].position[0], vertices.size(), sizeof(RawVertex), /*threshold*/ 1.01f);
        meshopt_optimizeVertexFetch(vertices.data(), indices.data(), indices.size(), vertices.data(), vertices.size(), sizeof(RawVertex));

    }

    Meshly SceneImporter::buildMeshlet(const meshopt_Meshlet& meshlet, const meshopt_Bounds& bounds)
    {
        Meshly meshly{};

        meshly.vertexOffset = meshlet.vertex_offset;
        meshly.triangleOffset = meshlet.triangle_offset;
        meshly.vertexCount = meshlet.vertex_count;
        meshly.triangleCount = meshlet.triangle_count;

        meshly.center[0] = bounds.center[0];
        meshly.center[1] = bounds.center[1];
        meshly.center[2] = bounds.center[2];
        meshly.radius = bounds.radius;

        meshly.coneAxis[0] = bounds.cone_axis_s8[0];
        meshly.coneAxis[1] = bounds.cone_axis_s8[1];
        meshly.coneAxis[2] = bounds.cone_axis_s8[2];
        meshly.coneCutoff = bounds.cone_cutoff_s8;

        return meshly;
    }

    glm::vec4 SceneImporter::calculateMeshBounds(aiMesh* mesh)
    {
        glm::vec4 meshBounds(0.0f);
        for (size_t i = 0; i < mesh->mNumVertices; ++i)
        {
            const aiVector3D& vertex = mesh->mVertices[i];
            meshBounds += glm::vec4(vertex.x, vertex.y, vertex.z, 0.0f);
        }
        meshBounds /= f32(mesh->mNumVertices);

        for (size_t i = 0; i < mesh->mNumVertices; ++i)
        {
            const aiVector3D& vertex = mesh->mVertices[i];
            meshBounds.w = glm::max(meshBounds.w, glm::distance(glm::vec3(meshBounds), glm::vec3(vertex.x, vertex.y, vertex.z)));
        }

        return meshBounds;
    }
}