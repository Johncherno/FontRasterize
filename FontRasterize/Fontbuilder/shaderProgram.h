#pragma once
#define GLEW_STATIC
#include<GL/glew.h>
#include<GLFW/glfw3.h>
#include<string>
#include<fstream>
#include<sstream>
#include<iostream>
#include<glm.hpp>
#include"gtc/type_ptr.hpp"
class shaderProgram
{
	enum  ShaderType
	{
		VERTEX,
		FRAGMENT,
		GEOMETRY,
		PROGRAM
	};
public:
	//������ɫ��
	bool loadShaders(const char* vsFilename, const char* fsFilename);
	void use();//�������ǵ���ɫ��
	void setUniform(const GLchar* name, const glm::vec2& v);
	void setUniform(const GLchar* name, const glm::vec3& v);
	void setUniform(const GLchar* name, const glm::vec4& v);
	void setUniform(const GLchar* name, const glm::mat4& m);
	void setUniform(const std::string& name, const glm::mat4& m);
	void setUniform(const std::string& name, const glm::vec3& m);
	void setUniform(const std::string& name, const glm::mat4 m[]);
	void setUniform(const GLchar* name, float& m);
	void setUniform(const std::string& name, const float(*m)[4]);
	void setUniformSampler(const GLchar* name, GLint slot);
	GLuint getprogram()const;
private:
	std::string fileToString(const std::string& filename);
	void checkCompileErrors(GLuint shader, ShaderType type);//��ȡ��ɫ������
	GLint getUniformLocation(const GLchar* name);
	GLuint mHandle;//��ɫ���������
	GLint result;//������Ϣ��ַ
	GLchar infoLog[512];//������Ϣ�ַ���
};

#pragma once
