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
	//加载着色器
	bool loadShaders(const char* vsFilename, const char* fsFilename);
	void use();//调用我们的着色器
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
	void checkCompileErrors(GLuint shader, ShaderType type);//读取着色器错误
	GLint getUniformLocation(const GLchar* name);
	GLuint mHandle;//着色器程序变量
	GLint result;//报错信息地址
	GLchar infoLog[512];//报错信息字符串
};

#pragma once
