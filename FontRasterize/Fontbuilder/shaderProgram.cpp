#include "shaderProgram.h"

bool shaderProgram::loadShaders(const char* vsFilename, const char* fsFilename)
{   //顶点着色器
    //vsfilename----->vertexshaderSrc
    std::string vsString = fileToString(vsFilename);//定义一个函数把文件转化为字符串
    const GLchar* vertexshaderSrc = vsString.c_str();//顶点着色器来源
    const GLchar* fragmentShaderSrc;//颜色着色器来源
    GLuint vs = glCreateShader(GL_VERTEX_SHADER);//创建顶点着色器
    glShaderSource(vs, 1, &vertexshaderSrc, NULL);//将输入字符串中所含顶点坐标  的字符串指针地址赋值到vs
    glCompileShader(vs);//获取vs地址进行编译
    checkCompileErrors(vs, VERTEX);//顶点着色器报错
    glGetShaderiv(vs, GL_COMPILE_STATUS, &result);
    if (!result)
    {
        glGetShaderInfoLog(vs, sizeof(infoLog), NULL, infoLog);
        std::cout << "error vertexShader failed compile" << infoLog << std::endl;
    }//打印出我们着色器错误

    //颜色着色器
    std::string fsString = fileToString(fsFilename);
    fragmentShaderSrc = fsString.c_str();
    GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);//创建颜色着色器
    glShaderSource(fs, 1, &fragmentShaderSrc, NULL);//将输入字符串中所含顶点坐标  的字符串指针地址赋值到fs
    glCompileShader(fs);//获取fs地址进行编译
    checkCompileErrors(fs, FRAGMENT);//颜色着色器报错
    glGetShaderiv(fs, GL_COMPILE_STATUS, &result);
    if (!result)
    {
        glGetShaderInfoLog(fs, sizeof(infoLog), NULL, infoLog);
        std::cout << "error fragmentShader failed compile" << infoLog << std::endl;
    }//打印出我们着色器错误
    mHandle = glCreateProgram();
    glAttachShader(mHandle, vs);
    glAttachShader(mHandle, fs);
    glLinkProgram(mHandle);//链接我们的着色器


    //做完这些工作删除掉这些地址变量
    glDeleteShader(vs);
    glDeleteShader(fs);
    return true;
}

void shaderProgram::use()//主函数调用着色器
{
    if (mHandle > 0)
    {
        glUseProgram(mHandle);

    }
}

std::string shaderProgram::fileToString(const std::string& filename)//文件转化为字符串
{
    std::stringstream ss;//字符流
    std::ifstream file;//文件名
    try
    {

        file.open(filename, std::ios::in);//打开文件
        if (!file.fail())
        {
            ss << file.rdbuf();//文件内容复制到字符流上
        }
        file.close();//关闭文件
    }
    catch (std::exception ex)
    {
        std::cerr << "error reading shader filename" << std::endl;//如果文件没有打开就报错
    }

    return ss.str();//把字符串传出去
}

void shaderProgram::checkCompileErrors(GLuint shader, ShaderType type)
{
    int status = 1;//状态默认为1
    if (type == PROGRAM)
    {
        glGetProgramiv(shader, GL_LINK_STATUS, &status);
        if (status == GL_FALSE)
        {
            GLint length = 0;
            glGetProgramiv(shader, GL_INFO_LOG_LENGTH, &length);
            std::string errorLog(length, ' ');
            glGetProgramInfoLog(shader, length, &length, &errorLog[0]);
            std::cerr << "error program link" << errorLog << std::endl;
        }
    }
    else//如果是顶点 颜色着色器 也要查出错误
    {
        glGetShaderiv(shader, GL_COMPILE_STATUS, &status);//OpenGL(glBindVertexArray()) 错误出现在之前的放入的参数为mhandle GL_LINK_STATUS   现在应为着色器 和 编译枚举参数   
        if (status == GL_FALSE)
        {
            GLint length = 0;
            glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &length);
            std::string errorLog(length, ' ');
            glGetShaderInfoLog(shader, length, &length, &errorLog[0]);
            std::cerr << "error shader compile" << errorLog << std::endl;
        }
    }

}
GLuint shaderProgram::getprogram()const
{
    return mHandle;
}
GLint shaderProgram::getUniformLocation(const GLchar* name)
{

    GLint it = glGetUniformLocation(mHandle, name);;
    return it;
}
void shaderProgram::setUniformSampler(const GLchar* name, GLint slot)
{
    auto loc = glGetUniformLocation(mHandle, name);
    glUniform1i(loc, slot);//绑定纹理

}
void shaderProgram::setUniform(const GLchar* name, const glm::vec2& v)
{
    GLint loc = getUniformLocation(name);
    glUniform2f(loc, v.x, v.y);
}
void shaderProgram::setUniform(const GLchar* name, const glm::vec3& v)
{
    GLint loc = getUniformLocation(name);
    glUniform3f(loc, v.x, v.y, v.z);
}
void shaderProgram::setUniform(const GLchar* name, const glm::vec4& v)
{
    GLint loc = getUniformLocation(name);
    glUniform4f(loc, v.x, v.y, v.z, v.w);
}
void shaderProgram::setUniform(const GLchar* name, const glm::mat4& m)
{
    GLint loc = getUniformLocation(name);
    glUniformMatrix4fv(loc, 1, GL_FALSE, &m[0][0]);
}
void shaderProgram::setUniform(const GLchar* name, float& m)
{
    GLint loc = getUniformLocation(name);
    glUniform1f(loc, m);
}
void shaderProgram::setUniform(const std::string& name, const float(*m)[4])
{
    GLint loc = getUniformLocation(name.c_str());
    glUniformMatrix4fv(loc, 1, GL_FALSE, &m[0][0]);
}
void shaderProgram::setUniform(const std::string& name, const glm::mat4& m)
{
    GLint loc = getUniformLocation(name.c_str());
    glUniformMatrix4fv(loc, 1, GL_FALSE, &m[0][0]);
}
void shaderProgram::setUniform(const std::string& name, const glm::vec3& m)
{
    GLint loc = getUniformLocation(name.c_str());
    glUniformMatrix4fv(loc, 1, GL_FALSE, &m[0]);
}
void shaderProgram::setUniform(const std::string& name, const glm::mat4 m[])
{
    GLint loc = getUniformLocation(name.c_str());
    glUniformMatrix4fv(loc, sizeof(m) / sizeof(glm::mat4), GL_FALSE, glm::value_ptr(m[0]));
}
