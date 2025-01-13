#include "shaderProgram.h"

bool shaderProgram::loadShaders(const char* vsFilename, const char* fsFilename)
{   //������ɫ��
    //vsfilename----->vertexshaderSrc
    std::string vsString = fileToString(vsFilename);//����һ���������ļ�ת��Ϊ�ַ���
    const GLchar* vertexshaderSrc = vsString.c_str();//������ɫ����Դ
    const GLchar* fragmentShaderSrc;//��ɫ��ɫ����Դ
    GLuint vs = glCreateShader(GL_VERTEX_SHADER);//����������ɫ��
    glShaderSource(vs, 1, &vertexshaderSrc, NULL);//�������ַ�����������������  ���ַ���ָ���ַ��ֵ��vs
    glCompileShader(vs);//��ȡvs��ַ���б���
    checkCompileErrors(vs, VERTEX);//������ɫ������
    glGetShaderiv(vs, GL_COMPILE_STATUS, &result);
    if (!result)
    {
        glGetShaderInfoLog(vs, sizeof(infoLog), NULL, infoLog);
        std::cout << "error vertexShader failed compile" << infoLog << std::endl;
    }//��ӡ��������ɫ������

    //��ɫ��ɫ��
    std::string fsString = fileToString(fsFilename);
    fragmentShaderSrc = fsString.c_str();
    GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);//������ɫ��ɫ��
    glShaderSource(fs, 1, &fragmentShaderSrc, NULL);//�������ַ�����������������  ���ַ���ָ���ַ��ֵ��fs
    glCompileShader(fs);//��ȡfs��ַ���б���
    checkCompileErrors(fs, FRAGMENT);//��ɫ��ɫ������
    glGetShaderiv(fs, GL_COMPILE_STATUS, &result);
    if (!result)
    {
        glGetShaderInfoLog(fs, sizeof(infoLog), NULL, infoLog);
        std::cout << "error fragmentShader failed compile" << infoLog << std::endl;
    }//��ӡ��������ɫ������
    mHandle = glCreateProgram();
    glAttachShader(mHandle, vs);
    glAttachShader(mHandle, fs);
    glLinkProgram(mHandle);//�������ǵ���ɫ��


    //������Щ����ɾ������Щ��ַ����
    glDeleteShader(vs);
    glDeleteShader(fs);
    return true;
}

void shaderProgram::use()//������������ɫ��
{
    if (mHandle > 0)
    {
        glUseProgram(mHandle);

    }
}

std::string shaderProgram::fileToString(const std::string& filename)//�ļ�ת��Ϊ�ַ���
{
    std::stringstream ss;//�ַ���
    std::ifstream file;//�ļ���
    try
    {

        file.open(filename, std::ios::in);//���ļ�
        if (!file.fail())
        {
            ss << file.rdbuf();//�ļ����ݸ��Ƶ��ַ�����
        }
        file.close();//�ر��ļ�
    }
    catch (std::exception ex)
    {
        std::cerr << "error reading shader filename" << std::endl;//����ļ�û�д򿪾ͱ���
    }

    return ss.str();//���ַ�������ȥ
}

void shaderProgram::checkCompileErrors(GLuint shader, ShaderType type)
{
    int status = 1;//״̬Ĭ��Ϊ1
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
    else//����Ƕ��� ��ɫ��ɫ�� ҲҪ�������
    {
        glGetShaderiv(shader, GL_COMPILE_STATUS, &status);//OpenGL(glBindVertexArray()) ���������֮ǰ�ķ���Ĳ���Ϊmhandle GL_LINK_STATUS   ����ӦΪ��ɫ�� �� ����ö�ٲ���   
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
    glUniform1i(loc, slot);//������

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
