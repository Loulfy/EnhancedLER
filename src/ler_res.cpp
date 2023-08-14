//
// Created by loulfy on 14/07/2023.
//

#include "ler_res.hpp"
#include "ler_dev.hpp"

namespace ler
{
    static const std::set<fs::path> c_supportedModels =
    {
        ".obj",
        ".fbx",
        ".glb",
        ".gltf"
    };

    bool AssimpFileSystem::exists(const fs::path& path) const
    {
        return aiScene->GetEmbeddedTexture(path.string().c_str()) != nullptr;
    }

    Blob AssimpFileSystem::readFile(const fs::path& path)
    {
        const aiTexture* em = aiScene->GetEmbeddedTexture(path.string().c_str());
        if (em == nullptr)
        {
            fs::path f = path.parent_path() / fs::path(path);
            return {};
        } else
        {
            const auto* buffer = reinterpret_cast<const unsigned char*>(em->pcData);
            return {buffer, buffer + em->mWidth};
        }
    }

    void AssimpFileSystem::enumerates(std::vector<fs::path>& entries)
    {
        for (size_t i = 0; i < aiScene->mNumTextures; ++i)
        {
            const aiTexture* texture = aiScene->mTextures[i];
            if (texture->mFilename.length > 0)
                entries.emplace_back(texture->mFilename.C_Str());
            else
                entries.emplace_back("*" + std::to_string(i));
        }
    }

    fs::file_time_type AssimpFileSystem::last_write_time(const fs::path& path)
    {
        return fs::last_write_time(path);
    }

    fs::path AssimpFileSystem::format_hint(const fs::path& path)
    {
        const aiTexture* em = aiScene->GetEmbeddedTexture(path.string().c_str());
        if (em)
        {
            std::string ext = ".";
            ext += em->achFormatHint;
            return ext;
        }
        return {};
    }

    glm::mat4 convert(aiMatrix4x4t<ai_real> from)
    {
        auto m = glm::make_mat4(from.Transpose()[0]);
        glm::quat rot = glm::quat_cast(m);
        return m;
    }

    physx::PxMat44 convert(const glm::mat4 from)
    {
        auto* glmData = const_cast<float*>(glm::value_ptr(from));
        return physx::PxMat44(glmData).getTranspose();
    }

    /*physx::PxTransform convertToPX(const aiMatrix4x4& aiMatrix)
    {
        // Extract the translation vector from aiMatrix4x4
        physx::PxVec3 translation(aiMatrix.a4, aiMatrix.b4, aiMatrix.c4);

        // Extract the rotation quaternion from aiMatrix4x4
        aiVector3D aiScaling;
        aiQuaternion rotation;
        aiMatrix.DecomposeNoScaling(rotation, aiScaling);
        rotation = rotation.Normalize();

        // Convert the rotation quaternion to PhysX PxQuat
        physx::PxQuat pxRotation(rotation.x, rotation.y, rotation.z, rotation.w);
        float magni = pxRotation.magnitude();
        bool finito = pxRotation.isFinite();
        if(!pxRotation.isSane())
            log::info("WTF");

        // Create the PxTransform
        return {translation, pxRotation};
    }*/

    physx::PxMeshScale convertToPxScale(const aiMatrix4x4& aiMatrix)
    {
        aiVector3D scaling;
        aiVector3D position;
        aiQuaternion rotation;
        aiMatrix.Decompose(scaling, rotation, position);
        return {physx::PxVec3(scaling.x, scaling.y, scaling.z)};
    }

    BatchedMesh::BatchedMesh()
    {
        auto s = Event::GetDispatcher().sink<Queue::CommandCompleteEvent>();
        s.connect<&BatchedMesh::receive>(this);
    }

    BatchedMesh::~BatchedMesh()
    {
        auto s = Event::GetDispatcher().sink<Queue::CommandCompleteEvent>();
        s.disconnect<&BatchedMesh::receive>(this);
    }

    void BatchedMesh::allocate(const LerDevicePtr& device)
    {
        indexBuffer = device->createBuffer(C64Mio, vk::BufferUsageFlagBits::eIndexBuffer);
        vertexBuffer = device->createBuffer(C64Mio, vk::BufferUsageFlagBits::eVertexBuffer);
        normalBuffer = device->createBuffer(C64Mio, vk::BufferUsageFlagBits::eVertexBuffer);
        tangentBuffer = device->createBuffer(C64Mio, vk::BufferUsageFlagBits::eVertexBuffer);
        texcoordBuffer = device->createBuffer(C64Mio, vk::BufferUsageFlagBits::eVertexBuffer);
        instanceBuffer = device->createBuffer(C32Mio, vk::BufferUsageFlagBits::eStorageBuffer);
        materialBuffer = device->createBuffer(C32Mio, vk::BufferUsageFlagBits::eStorageBuffer);
        meshletBuffer = device->createBuffer(C16Mio, vk::BufferUsageFlagBits::eStorageBuffer);
        commandBuffer = device->createBuffer(C16Mio, vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eIndirectBuffer);
        aabbBuffer = device->createBuffer(C16Mio, vk::BufferUsageFlagBits::eVertexBuffer);

        m_textures = device->getTexturePool();
        renderList.allocate(device);
    }

    void BatchedMesh::append(LerDevicePtr& device, FsTag tag, const fs::path& path)
    {
        auto ext = FileSystemService::Get(tag)->format_hint(path);
        if(c_supportedModels.contains(ext))
            Async::GetPool().push_task(processMeshes, device, tag, path, &m_importer, this);
    }

    void BatchedMesh::registerSubmission(uint64_t submissionId, const TexturePool::Require& key)
    {
        m_submissions.emplace_back(submissionId, key);
    }

    void BatchedMesh::loadSceneGraph(flecs::world& world)
    {
        loadNode(&world, m_importer.GetScene()->mRootNode, 0, *this);
    }

    void BatchedMesh::receive(const Queue::CommandCompleteEvent& e)
    {

    }

    bool BatchedMesh::checkUpdate(LerDevicePtr& device)
    {
        size_t size = m_submissions.size();
        std::erase_if(m_submissions, [&](const auto& sub){
            return device->pollCommand(sub.first) && m_textures->verify(sub.second);
        });
        return size > m_submissions.size();
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

    void transformBoundingBox(const glm::mat4& t, glm::vec3& min, glm::vec3& max)
    {
        std::array<glm::vec3, 8> pts = {
                glm::vec3(min.x, min.y, min.z),
                glm::vec3(min.x, max.y, min.z),
                glm::vec3(min.x, min.y, max.z),
                glm::vec3(min.x, max.y, max.z),
                glm::vec3(max.x, min.y, min.z),
                glm::vec3(max.x, max.y, min.z),
                glm::vec3(max.x, min.y, max.z),
                glm::vec3(max.x, max.y, max.z),
        };

        for (auto& p: pts)
            p = glm::vec3(t * glm::vec4(p, 1.f));

        // create
        min = glm::vec3(std::numeric_limits<float>::max());
        max = glm::vec3(std::numeric_limits<float>::lowest());

        for (auto& p : pts)
        {
            min = glm::min(min, p);
            max = glm::max(max, p);
        }
    }

    std::array<glm::vec3, 8> createBox(const MeshInfo* mesh, glm::mat4 model)
    {
        glm::vec3 min = mesh->bMin;
        glm::vec3 max = mesh->bMax;
        std::array<glm::vec3, 8> pts = {
                glm::vec3(max.x, max.y, max.z),
                glm::vec3(max.x, max.y, min.z),
                glm::vec3(max.x, min.y, max.z),
                glm::vec3(max.x, min.y, min.z),
                glm::vec3(min.x, max.y, max.z),
                glm::vec3(min.x, max.y, min.z),
                glm::vec3(min.x, min.y, max.z),
                glm::vec3(min.x, min.y, min.z),
        };
        for (auto& p: pts)
            p = glm::vec3(model * glm::vec4(p, 1.f));
        return pts;
    }

    void BatchedMesh::loadNode(flecs::world* world, aiNode* aiNode, uint32_t meshOffset, BatchedMesh& batch)
    {
        if(aiNode == nullptr)
            return;

        for (size_t i = 0; i < aiNode->mNumMeshes; ++i)
        {
            auto node = world->entity();
            batch.drawCount++;
            node.set<mesh>({aiNode->mMeshes[i]+meshOffset});

            const auto& ind = batch.meshes[aiNode->mMeshes[i]];
            aiMatrix4x4 model = aiNode->mTransformation;
            auto* currentParent = aiNode->mParent;
            while (currentParent) {
                model = currentParent->mTransformation * model;
                currentParent = currentParent->mParent;
            }

            auto t = convert(model);

            glm::vec3 min = ind->bMin;
            glm::vec3 max = ind->bMax;
            min = glm::vec3(t * glm::vec4(min, 1.f));
            max = glm::vec3(t * glm::vec4(max, 1.f));
            glm::vec3 c = (min+max)/2.f;

            //t = glm::translate(t, c);

            node.set<transform>({t, t});

            ShapeVisitor visitor(ind, model);
            visitor.visit();

            node.set<physic>({visitor.actor});
            auto tag = new UserData;
            tag->e = node;
            visitor.actor->userData = static_cast<void*>(tag);

            //physx::PxShape* shape = ind->collision;
            /*if(ind->collision)
            {
                auto pxt = convertToPX(model);
                physx::PxRigidStatic* actor = PxGetPhysics().createRigidStatic(pxt);
                actor->setName(aiNode->mName.C_Str());

                //auto* material = PxGetPhysics().createMaterial(0.5f, 0.5f, 0.6f);
                //physx::PxTriangleMeshGeometry geometry(triMesh);
                //physx::PxConvexMeshGeometry geometry(triMesh);
                //physx::PxRigidActorExt::createExclusiveShape(*actor, geometry, *material);
                //actor->attachShape(*shape);

                node.set<physic>({actor});
                //if(pxt.isSane())
                    //actor->setGlobalPose(pxt);
                auto tag = new UserData;
                tag->e = node;
                actor->userData = static_cast<void*>(tag);
            }*/
        }

        for(size_t i = 0; i < aiNode->mNumChildren; ++i)
            loadNode(world, aiNode->mChildren[i], meshOffset, batch);
    }

    void BatchedMesh::processMeshes(LerDevicePtr& device, FsTag tag, const fs::path& path, Assimp::Importer* imp, BatchedMesh* batch)
    {
        //Assimp::Importer importer;
        unsigned int postProcess = aiProcessPreset_TargetRealtime_Fast;
        postProcess |= aiProcess_ConvertToLeftHanded;
        postProcess |= aiProcess_GenBoundingBoxes;
        const aiScene* aiScene;
        if (tag == FsTag_Default)
        {
            aiScene = imp->ReadFile(path.string(), postProcess);
        } else
        {
            const auto blob = FileSystemService::Get().readFile(tag, path);
            aiScene = imp->ReadFileFromMemory(blob.data(), blob.size(), postProcess, path.string().c_str());
        }
        if (aiScene == nullptr || aiScene->mNumMeshes < 0)
        {
            log::error(imp->GetErrorString());
            return;
        }

        std::vector<fs::path> entries;
        if (aiScene->mNumTextures == 0)
            FileSystemService::Get().mount(FsTag_Assimp, StdFileSystem::Create(ASSETS_DIR)); //StdFileSystem::Create(path.parent_path()));
        else
            FileSystemService::Get().mount(FsTag_Assimp, AssimpFileSystem::Create(aiScene));

        TexturePoolPtr pool = device->getTexturePool();

        std::set<std::string> cache;
        TexturePool::Require key;
        std::vector<Material> materials;
        materials.reserve(aiScene->mNumMaterials);
        for (size_t i = 0; i < aiScene->mNumMaterials; ++i)
        {
            aiString filename;
            aiColor3D baseColor;
            Material materialInstance;
            auto* material = aiScene->mMaterials[i];
            material->Get(AI_MATKEY_COLOR_DIFFUSE, baseColor);
            materialInstance.color = glm::vec3(baseColor.r, baseColor.g, baseColor.b);
            if (material->GetTextureCount(aiTextureType_BASE_COLOR) > 0)
            {
                material->GetTexture(aiTextureType_BASE_COLOR, 0, &filename);
                materialInstance.texId = pool->fetch(filename.C_Str(), FsTag_Assimp);
                key.set(materialInstance.texId);
            }
            if (material->GetTextureCount(aiTextureType_AMBIENT) > 0)
            {
                material->GetTexture(aiTextureType_AMBIENT, 0, &filename);
                materialInstance.texId = pool->fetch(filename.C_Str(), FsTag_Assimp);
                key.set(materialInstance.texId);
            }
            if (material->GetTextureCount(aiTextureType_DIFFUSE) > 0)
            {
                material->GetTexture(aiTextureType_DIFFUSE, 0, &filename);
                materialInstance.texId = pool->fetch(filename.C_Str(), FsTag_Assimp);
                key.set(materialInstance.texId);
            }
            if (material->GetTextureCount(aiTextureType_NORMALS) > 0)
            {
                material->GetTexture(aiTextureType_NORMALS, 0, &filename);
                materialInstance.norId = pool->fetch(filename.C_Str(), FsTag_Assimp);
                key.set(materialInstance.norId);
            }
            materials.emplace_back(materialInstance);
        }

        SubmitTransfer sub;
        uint32_t meshOffset = batch->meshCount.fetch_add(aiScene->mNumMeshes);

        uint32_t indexCount = 0;
        uint32_t vertexCount = 0;
        for (size_t i = 0; i < aiScene->mNumMeshes; ++i)
        {
            aiMesh* mesh = aiScene->mMeshes[i];
            batch->meshes[i + meshOffset] = std::make_shared<MeshInfo>();
            MeshInfo* ind = batch->meshes[i + meshOffset].get();
            //log::debug("Mesh: {}", mesh->mName.C_Str());
            ind->countIndex = mesh->mNumFaces * 3;
            ind->firstIndex = indexCount;
            ind->countVertex = mesh->mNumVertices;
            ind->firstVertex = static_cast<int32_t>(vertexCount);
            ind->bMin = glm::make_vec3(&mesh->mAABB.mMin[0]);
            ind->bMax = glm::make_vec3(&mesh->mAABB.mMax[0]);
            ind->materialId = mesh->mMaterialIndex;
            ind->name = mesh->mName.C_Str();

            indexCount += ind->countIndex;
            vertexCount += ind->countVertex;

            std::vector<uint32_t> indices;
            indices.reserve(mesh->mNumFaces*3);
            for (size_t j = 0; j < mesh->mNumFaces; ++j)
                indices.insert(indices.end(), mesh->mFaces[j].mIndices, mesh->mFaces[j].mIndices + 3);

            physx::PxTriangleMeshDesc meshDesc;
            meshDesc.points.count           = mesh->mNumVertices;
            meshDesc.points.stride          = sizeof(physx::PxVec3);
            meshDesc.points.data            = mesh->mVertices;

            meshDesc.triangles.count        = mesh->mNumFaces;
            meshDesc.triangles.stride       = 3*sizeof(physx::PxU32);
            meshDesc.triangles.data         = indices.data();

            physx::PxConvexMeshDesc convexDesc;
            convexDesc.points.count     = mesh->mNumVertices;
            convexDesc.points.stride    = sizeof(physx::PxVec3);
            convexDesc.points.data      = mesh->mVertices;
            convexDesc.flags            = physx::PxConvexFlag::eCOMPUTE_CONVEX | physx::PxConvexFlag::eDISABLE_MESH_VALIDATION | physx::PxConvexFlag::eFAST_INERTIA_COMPUTATION;

            //ind->collision = PXInitializer::BuildConvexMesh(convexDesc);
            //ind->collision = PXInitializer::BuildTriangleMesh(meshDesc);
        }

        //::ler::loadNode(batch, aiScene->mRootNode);

        uint32_t indexDstOffset = batch->indexCount.fetch_add(indexCount);
        uint32_t vertexDstOffset = batch->vertexCount.fetch_add(vertexCount);

        std::vector<Meshlet> meshes(aiScene->mNumMeshes);
        for (size_t i = 0; i < aiScene->mNumMeshes; ++i)
        {
            MeshInfo* ind = batch->meshes[i + meshOffset].get();
            ind->firstIndex += indexDstOffset;
            ind->firstVertex += static_cast<int32_t>(vertexDstOffset);

            // TODO: meshlet append
            meshes[i].countIndex = ind->countIndex;
            meshes[i].firstIndex = ind->firstIndex;
            meshes[i].vertexOffset = ind->firstVertex;
        }

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

        BufferPtr staging = device->createBuffer(C128Mio*1.5, vk::BufferUsageFlagBits(), true);
        void* data = staging->hostInfo.pMappedData;
        auto* dest = static_cast<std::byte*>(data);

        /*staging->uploadFromMemory(batch->instances.data(), batch->instances.size()*sizeof(Instance));
        auto cmd = device->createCommand(CommandQueue::Transfer);
        cmd->copyBuffer(staging, batch->instanceBuffer, batch->instances.size()*sizeof(Instance), 0);
        device->submitAndWait(cmd);*/

        /*auto cmd = device->createCommand(CommandQueue::Transfer);
        staging->uploadFromMemory(batch->materials.data(), batch->materials.size()*sizeof(Material));
        cmd = device->createCommand(CommandQueue::Transfer);
        cmd->copyBuffer(staging, batch->materialBuffer, batch->materials.size()*sizeof(Material), 0);
        device->submitAndWait(cmd);*/

        indexDstOffset *= sizeof(uint32_t);
        vertexDstOffset *= sizeof(glm::vec3);

        std::array<vk::BufferCopy, 7> copies;
        copies[0].size = indices.size() * sizeof(uint32_t);

        size_t offset = 0;
        copies[0].srcOffset = offset;
        copies[0].dstOffset = indexDstOffset;
        std::memcpy(dest, indices.data(), copies[0].size);

        offset += copies[0].size;
        copies[1].srcOffset = offset;
        copies[1].dstOffset = vertexDstOffset;
        populateBufferCopy(dest, copies[1], aiScene, [](aiMesh* mesh)
        { return mesh->HasPositions(); }, [](aiMesh* mesh)
                           { return mesh->mVertices; });

        offset += copies[1].size;
        copies[2].srcOffset = offset;
        copies[2].dstOffset = vertexDstOffset;
        populateBufferCopy(dest, copies[2], aiScene, [](aiMesh* mesh)
        { return mesh->HasNormals(); }, [](aiMesh* mesh)
                           { return mesh->mNormals; });

        offset += copies[2].size;
        copies[3].srcOffset = offset;
        copies[3].dstOffset = vertexDstOffset;
        populateBufferCopy(dest, copies[3], aiScene, [](aiMesh* mesh)
        { return mesh->HasTextureCoords(0); }, [](aiMesh* mesh)
                           { return mesh->mTextureCoords[0]; });

        offset += copies[3].size;
        copies[4].srcOffset = offset;
        copies[4].dstOffset = vertexDstOffset;
        populateBufferCopy(dest, copies[4], aiScene, [](aiMesh* mesh)
        { return mesh->HasTangentsAndBitangents(); }, [](aiMesh* mesh)
                           { return mesh->mTangents; });

        offset += copies[4].size;
        copies[5].srcOffset = offset;
        copies[5].dstOffset = vertexDstOffset; //TODO: append invalid
        copies[5].size = materials.size()*sizeof(Material);
        std::memcpy(dest + copies[5].srcOffset, materials.data(), copies[5].size);

        offset += copies[5].size;
        copies[6].srcOffset = offset;
        copies[6].dstOffset = vertexDstOffset; //TODO: append invalid
        copies[6].size = meshes.size()*sizeof(Meshlet);
        std::memcpy(dest + copies[6].srcOffset, meshes.data(), copies[6].size);

        sub.command = device->createCommand(CommandQueue::Transfer);
        sub.command->copyBuffer(staging, batch->indexBuffer, copies[0]);
        sub.command->copyBuffer(staging, batch->vertexBuffer, copies[1]);
        sub.command->copyBuffer(staging, batch->normalBuffer, copies[2]);
        sub.command->copyBuffer(staging, batch->texcoordBuffer, copies[3]);
        sub.command->copyBuffer(staging, batch->tangentBuffer, copies[4]);
        sub.command->copyBuffer(staging, batch->materialBuffer, copies[5]);
        sub.command->copyBuffer(staging, batch->meshletBuffer, copies[6]);

        sub.dependency = key;
        sub.batch = batch;
        AsyncQueue<AsyncRequest>::Commit(sub);
        log::debug("Submit async meshes: {}", path.string());
    }

    TexturePool::TexturePool()
    {
        auto s = Event::GetDispatcher().sink<Queue::CommandCompleteEvent>();
        s.connect<&TexturePool::receive>(this);
    }

    TexturePool::~TexturePool()
    {
        auto s = Event::GetDispatcher().sink<Queue::CommandCompleteEvent>();
        s.disconnect<&TexturePool::receive>(this);
    }

    uint32_t TexturePool::fetch(const fs::path& filename, FsTag tag)
    {
        const fs::path ext = FileSystemService::Get(tag)->format_hint(filename);
        if(ImageLoader::support(ext))
        {
            uint32_t index = allocate();
            Resource res(tag, index, filename);
            Async::GetPool().push_task(processImages, m_device, std::move(res));
            return index;
        }

        return 0;
    }

    uint32_t TexturePool::allocate()
    {
        if(m_textureCount == Size)
            log::exit("TexturePool Size exceeded");
        return m_textureCount.fetch_add(1, std::memory_order_relaxed);
    }

    void TexturePool::receive(const Queue::CommandCompleteEvent& e)
    {
        auto range = m_submitted.equal_range(e.submissionId);
        for (auto i = range.first; i != range.second; ++i)
            m_loadedTextures.set(i->second);

        m_submitted.erase(e.submissionId);
    }

    std::span<TexturePtr> TexturePool::getTextures()
    {
        return {m_textures.data(), m_textureCount};
    }

    std::vector<vk::ImageView> TexturePool::getImageViews()
    {
        auto views = m_textures | std::views::take(int(m_textureCount)) | std::views::transform([](const TexturePtr& tex) { return tex->view(); });
        return {views.begin(), views.end()};
    }

    std::vector<std::string> TexturePool::getTextureList() const
    {
        auto kv = std::views::keys(m_cache);
        return {kv.begin(), kv.end()};
    }

    TexturePtr TexturePool::getTextureFromIndex(uint32_t index) const
    {
        if (m_textures.size() > index)
            return m_textures[index];
        return {};
    }

    const std::unordered_multimap<std::string, uint32_t>& TexturePool::getTextureCacheRef() const
    {
        return std::ref(m_cache);
    }

    void TexturePool::set(uint32_t index, const TexturePtr& texture, uint64_t ticket)
    {
        if (m_textures.size() > index && texture)
        {
            m_textures[index] = texture;
            m_cache.emplace(texture->name, index);
            m_submitted.emplace(ticket, index);
        }
    }

    void TexturePool::processImages(LerDevice* device, const Resource& res)
    {
        ImagePtr img = ImageLoader::load(FileSystemService::Get(res.tag), res.path);

        vk::Extent2D extent = img->extent();
        size_t imageSize = img->byteSize();
        TexturePtr texture = device->createTexture(img->format(), extent);
        BufferPtr staging = device->createBuffer(imageSize, vk::BufferUsageFlagBits(), true);
        staging->uploadFromMemory(img->data(), imageSize);
        texture->name = res.path.string();

        CommandPtr cmd = device->createCommand(CommandQueue::Transfer);
        cmd->copyBufferToTexture(staging, texture);

        SubmitTexture submit;
        submit.id = res.id;
        submit.command = cmd;
        submit.texture = texture;
        AsyncQueue<AsyncRequest>::Commit(submit);
        log::debug("Submit async images: {}", res.path.string());
    }
}