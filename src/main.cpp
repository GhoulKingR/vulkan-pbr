#include "engine.hpp"
#include "glm/ext/matrix_float4x4.hpp"
#include "glm/ext/matrix_transform.hpp"
#include "glm/ext/vector_float3.hpp"
#include "glm/ext/vector_float4.hpp"
#include "glm/trigonometric.hpp"

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <print>
#include <stdexcept>
#include <vector>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

int main() {
    try {
        Engine njin;

        const auto viewpos = glm::vec3{-10.0, -10.0, -10.0};
        const auto cubeVertices = std::vector<Engine::Vertex>{
            // front face
            {{-1.0, -1.0, -1.0}, { 0.0,  0.0, -1.0}},   // 0
            {{-1.0,  1.0, -1.0}, { 0.0,  0.0, -1.0}},   // 1
            {{ 1.0,  1.0, -1.0}, { 0.0,  0.0, -1.0}},   // 2
            {{ 1.0, -1.0, -1.0}, { 0.0,  0.0, -1.0}},   // 3

            // top face
            {{-1.0, -1.0,  1.0}, { 0.0, -1.0,  0.0}},   // 4
            {{-1.0, -1.0, -1.0}, { 0.0, -1.0,  0.0}},   // 5
            {{ 1.0, -1.0, -1.0}, { 0.0, -1.0,  0.0}},   // 6
            {{ 1.0, -1.0,  1.0}, { 0.0, -1.0,  0.0}},   // 7

            // back face
            {{-1.0, -1.0,  1.0}, { 0.0,  0.0,  1.0}},   // 8
            {{-1.0,  1.0,  1.0}, { 0.0,  0.0,  1.0}},   // 9
            {{ 1.0,  1.0,  1.0}, { 0.0,  0.0,  1.0}},   // 10
            {{ 1.0, -1.0,  1.0}, { 0.0,  0.0,  1.0}},   // 11

            // bottom face
            {{-1.0,  1.0, -1.0}, { 0.0,  1.0,  0.0}},   // 12
            {{ 1.0,  1.0, -1.0}, { 0.0,  1.0,  0.0}},   // 13
            {{-1.0,  1.0,  1.0}, { 0.0,  1.0,  0.0}},   // 14
            {{ 1.0,  1.0,  1.0}, { 0.0,  1.0,  0.0}},   // 15

            // left face
            {{-1.0, -1.0, -1.0}, {-1.0,  0.0,  0.0}},   // 16
            {{-1.0,  1.0, -1.0}, {-1.0,  0.0,  0.0}},   // 17
            {{-1.0,  1.0,  1.0}, {-1.0,  0.0,  0.0}},   // 18
            {{-1.0, -1.0,  1.0}, {-1.0,  0.0,  0.0}},   // 19

            // right face
            {{ 1.0, -1.0, -1.0}, { 1.0,  0.0,  0.0}},   // 20
            {{ 1.0,  1.0, -1.0}, { 1.0,  0.0,  0.0}},   // 21
            {{ 1.0,  1.0,  1.0}, { 1.0,  0.0,  0.0}},   // 22
            {{ 1.0, -1.0,  1.0}, { 1.0,  0.0,  0.0}},   // 23
        };
        const auto cubeIndices = std::vector<uint32_t>{
            0, 1, 2, 2, 3, 0,       // front face
            4, 5, 6, 6, 7, 4,       // top face
            8, 9, 10, 10, 11, 8,    // back face
            14, 12, 13, 13, 15, 14, // bottom face
            16, 19, 18, 18, 17, 16, // left face
            20, 21, 22, 22, 23, 20, // right face
        };

        Engine::Mesh lightMesh(cubeVertices, cubeIndices);
        lightMesh.setCamera(viewpos, {0.0, 0.0, 0.0}, {0.0, -1.0, 0.0});
        lightMesh.setMaterial(
            {1.0, 1.0, 1.0},
            0.0, 1.0, 1.0
        );

        Engine::Mesh mesh(cubeVertices, cubeIndices);
        mesh.setCamera(viewpos, {0.0, 0.0, 0.0}, {0.0, -1.0, 0.0});
        mesh.setMaterial(
            {0.01, 0.01, 0.01},
            0.0, 1.0, 0.01
        );

        Engine::Mesh floor(
            {
                {{-50.0, 0.0,  50.0}, { 0.0, -1.0,  0.0}},   // 0
                {{-50.0, 0.0, -50.0}, { 0.0, -1.0,  0.0}},   // 1
                {{ 50.0, 0.0, -50.0}, { 0.0, -1.0,  0.0}},   // 2
                {{ 50.0, 0.0,  50.0}, { 0.0, -1.0,  0.0}},   // 3
            },
            { 0, 1, 2, 2, 3, 0 }
        );
        floor.setTransform([](){
            auto model = glm::identity<glm::mat4>();
            model = glm::translate(model, {0.0, 1.01, 0.0});
            return model;
        }());
        floor.setCamera(viewpos, {0.0, 0.0, 0.0}, {0.0, -1.0, 0.0});
        floor.setColor({0.5, 0.8, 0.8});
        floor.setMaterial({0.2, 0.5, 0.1}, 1.0, 0.0, 0.01);

        constexpr uint16_t LIGHT_ROTATION_SPEED = 30.0f;
        auto beginTime = std::chrono::high_resolution_clock::now();
        auto lightPos = glm::vec3{-4.0, -2.0, -4.0}; 

        while (njin.running()) {
            glfwPollEvents();

            mesh.setLight(lightPos);
            floor.setLight(lightPos);
            lightMesh.setLight(lightPos);
            // lightMesh.setLight({0.0, 0.0, 0.0});

            lightMesh.setTransform([&lightPos](){
                auto model = glm::identity<glm::mat4>();
                model = glm::translate(model, lightPos);
                model = glm::scale(model, glm::vec3{0.25});
                return model;
            }());

            if (!njin.beginFrame()) continue;
                mesh.draw();
                floor.draw();
                lightMesh.draw();
            njin.endFrame();

            // update light pos after rendering
            auto now = std::chrono::high_resolution_clock::now();
            float deltaTime = std::chrono::duration<float, std::chrono::seconds::period>(now - beginTime).count();
            auto rotationMat = glm::rotate(
                glm::identity<glm::mat4>(),
                glm::radians(LIGHT_ROTATION_SPEED * deltaTime),
                {0.0, -1.0, 0.0}
            );
            lightPos = rotationMat * glm::vec4(lightPos, 1.0);
            beginTime = now;
        }

        njin.wait();
    } catch (const std::runtime_error& e) {
        std::println(stderr, "[Error] {}", e.what());
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
