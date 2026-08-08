#pragma once
#include <vulkan/vulkan.h>
#include <array>
#include <cstdint>
#include <vector>

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/mat4x4.hpp>
#include "glm/ext/matrix_float3x3.hpp"
#include "glm/ext/vector_float3.hpp"

constexpr uint32_t WIDTH = 800;
constexpr uint32_t HEIGHT = 600;
constexpr int MAX_FRAMES_IN_FLIGHT = 2;

struct Engine {
    Engine();
    ~Engine();
    bool running();
    void wait();
    bool beginFrame();
    void endFrame();

    struct Vertex {
        glm::vec3 pos;
        glm::vec3 normal;

        bool operator==(const Vertex& other) const {
            return pos == other.pos && normal == other.normal;
        }
    };

private:
    struct ProjectionUBO {
        glm::mat4 model;
        glm::mat4 view;
        glm::mat4 proj;

        glm::vec3 lightPos;
        glm::mat3 model_normal;
    };
    struct UBO {
        glm::vec3 color{1.0f};
    };

public:
    class Mesh {
        UBO fragmentData;
        ProjectionUBO projectionData;

        // buffers and other requirements
        uint32_t indexCount;
        VkBuffer vertexBuffer;
        VkBuffer indexBuffer;
        VkDeviceMemory vertexBufferMemory;
        VkDeviceMemory indexBufferMemory;

        // uniform buffers
        std::array<VkBuffer, MAX_FRAMES_IN_FLIGHT> projectionUniformBuffers;
        std::array<VkDeviceMemory, MAX_FRAMES_IN_FLIGHT> projectionUniformBufferMemories;
        std::array<void*, MAX_FRAMES_IN_FLIGHT> mappedProjectionUniformBufferMemories;
        std::array<VkBuffer, MAX_FRAMES_IN_FLIGHT> uniformBuffers;
        std::array<VkDeviceMemory, MAX_FRAMES_IN_FLIGHT> uniformBufferMemories;
        std::array<void*, MAX_FRAMES_IN_FLIGHT> mappedUniformBufferMemories;

        // descriptor set stuffs
        VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
        std::array<VkDescriptorSet, MAX_FRAMES_IN_FLIGHT> descriptorSets;
    
    public:
        Mesh(
            const std::vector<Vertex> &vertices,
            const std::vector<uint32_t> &indices
        );
        ~Mesh();
        void draw();
        void setCamera(glm::mat4 view);
        void setTransform(glm::vec3 pos, glm::vec3 scale, glm::vec3 rotate);
        void setColor(glm::vec3 color);
        void setLight(glm::vec3 light);
    };
};
