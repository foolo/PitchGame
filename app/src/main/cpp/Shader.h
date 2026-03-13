#ifndef ANDROIDGLINVESTIGATIONS_SHADER_H
#define ANDROIDGLINVESTIGATIONS_SHADER_H

#include <string>
#include <GLES3/gl3.h>

class Model;

/*!
 * A class representing a simple shader program. It consists of vertex and fragment components.
 */
class Shader {
public:
    static Shader *loadShader(
            const std::string &vertexSource,
            const std::string &fragmentSource,
            const std::string &positionAttributeName,
            const std::string &uvAttributeName,
            const std::string &projectionMatrixUniformName,
            const std::string &offsetUniformName = "");

    inline ~Shader() {
        if (program_) {
            glDeleteProgram(program_);
            program_ = 0;
        }
    }

    void activate() const;
    void deactivate() const;
    void drawModel(const Model &model) const;
    void setProjectionMatrix(float *projectionMatrix) const;
    void setOffset(float x, float y) const;

    inline GLuint getProgram() const { return program_; }

private:
    static GLuint loadShader(GLenum shaderType, const std::string &shaderSource);

    constexpr Shader(
            GLuint program,
            GLint position,
            GLint uv,
            GLint projectionMatrix,
            GLint offset)
            : program_(program),
              position_(position),
              uv_(uv),
              projectionMatrix_(projectionMatrix),
              offset_(offset) {}

    GLuint program_;
    GLint position_;
    GLint uv_;
    GLint projectionMatrix_;
    GLint offset_;
};

#endif //ANDROIDGLINVESTIGATIONS_SHADER_H
