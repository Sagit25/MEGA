#ifndef OPENGL_UTILS_H
#define OPENGL_UTILS_H

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <vector>

class VAO {
public:
    unsigned int ID = 0;
    unsigned int associatedVBOID = 0;
    unsigned int associatedEBOID = 0;
    unsigned int vertexNumber = 0;
};

VAO* getVAOFromAttribData(const std::vector<float>& attrib_data, const std::vector<unsigned int>& attrib_sizes,
    const std::vector<unsigned int>& indices_data = std::vector<unsigned int>())
{
    VAO* vao = new VAO();
    glGenVertexArrays(1, &(vao->ID));
    glGenBuffers(1, &(vao->associatedVBOID));

    glBindVertexArray(vao->ID);
    glBindBuffer(GL_ARRAY_BUFFER, vao->associatedVBOID);
    glBufferData(GL_ARRAY_BUFFER, attrib_data.size() * sizeof(float), attrib_data.data(), GL_STATIC_DRAW);

    if (!indices_data.empty()) {
        glGenBuffers(1, &(vao->associatedEBOID));
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, vao->associatedEBOID);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices_data.size() * sizeof(unsigned int), indices_data.data(), GL_STATIC_DRAW);
    }

    unsigned int offset = 0;
    unsigned int index = 0;
    unsigned int size_sum = 0;
    for (auto const& value : attrib_sizes) {
        size_sum += value;
    }

    vao->vertexNumber = size_sum > 0 ? static_cast<unsigned int>(attrib_data.size()) / size_sum : 0;
    if (!indices_data.empty()) {
        vao->vertexNumber = static_cast<unsigned int>(indices_data.size());
    }

    for (auto const& value : attrib_sizes) {
        glVertexAttribPointer(index, value, GL_FLOAT, GL_FALSE, size_sum * sizeof(float), (void*)(offset * sizeof(float)));
        glEnableVertexAttribArray(index);
        index++;
        offset += value;
    }

    return vao;
}

void getPositionVAO(const float* vertices, unsigned int size, unsigned int& VAO, unsigned int& VBO)
{
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);

    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, size, vertices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
}

#endif
