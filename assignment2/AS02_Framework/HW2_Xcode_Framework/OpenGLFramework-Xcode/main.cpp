#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <math.h>
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include "textfile.h"

#include "Vectors.h"
#include "Matrices.h"
#define TINYOBJLOADER_IMPLEMENTATION
#include "tiny_obj_loader.h"

#ifndef max
# define max(a,b) (((a)>(b))?(a):(b))
# define min(a,b) (((a)<(b))?(a):(b))
#endif

using namespace std;

// Default window size
int WINDOW_WIDTH = 800;
int WINDOW_HEIGHT = 800;

bool mouse_pressed = false;
double starting_press_x = -1;
double starting_press_y = -1;

enum TransMode
{
	GeoTranslation = 0,
	GeoRotation = 1,
	GeoScaling = 2,
	ViewCenter = 3,
	ViewEye = 4,
	ViewUp = 5,
	LightEdit = 6,
	ShininessEdit = 7
};

GLuint ShaderID;

vector<string> filenames; // .obj filename list

struct PhongMaterial
{
	Vector3 Ka;
	Vector3 Kd;
	Vector3 Ks;
};

typedef struct
{
	GLuint vao;
	GLuint vbo;
	GLuint vboTex;
	GLuint ebo;
	GLuint p_color;
	int vertex_count;
	GLuint p_normal;
	PhongMaterial material;
	int indexCount;
	GLuint m_texture;
} Shape;

struct model
{
	Vector3 position = Vector3(0, 0, 0);
	Vector3 scale = Vector3(1, 1, 1);
	Vector3 rotation = Vector3(0, 0, 0);	// Euler form

	vector<Shape> shapes;
};
vector<model> models;

struct camera
{
	Vector3 position;
	Vector3 center;
	Vector3 up_vector;
};
camera main_camera;

struct project_setting
{
	GLfloat nearClip, farClip;
	GLfloat fovy;
	GLfloat aspect;
	GLfloat left, right, top, bottom;
};
project_setting proj;

enum ProjMode
{
	Orthogonal = 0,
	Perspective = 1,
};
ProjMode cur_proj_mode = Orthogonal;
TransMode cur_trans_mode = GeoTranslation;

Matrix4 view_matrix;
Matrix4 project_matrix;

Shape quad;
Shape m_shpae;
int cur_idx = 0; // represent which model should be rendered now
int cur_light_idx = 0;

// all changable light attribute
struct light_attribute{
	Vector3 directional_position;
	Vector3 directional_diffuse;
	Vector3 point_position;
	Vector3 point_diffuse;
	Vector3 spot_position;
	float spot_cutoff;
	float shininess;
};
light_attribute lightAtt;

static GLvoid Normalize(GLfloat v[3])
{
	GLfloat l;

	l = (GLfloat)sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
	v[0] /= l;
	v[1] /= l;
	v[2] /= l;
}

static GLvoid Cross(GLfloat u[3], GLfloat v[3], GLfloat n[3])
{

	n[0] = u[1] * v[2] - u[2] * v[1];
	n[1] = u[2] * v[0] - u[0] * v[2];
	n[2] = u[0] * v[1] - u[1] * v[0];
}


// [TODO] given a translation vector then output a Matrix4 (Translation Matrix)
Matrix4 translate(Vector3 vec)
{
	Matrix4 mat;


	mat = Matrix4(
		1, 0, 0, vec.x,
		0, 1, 0, vec.y,
		0, 0, 1, vec.z,
		0, 0, 0, 1
	);


	return mat;
}

// [TODO] given a scaling vector then output a Matrix4 (Scaling Matrix)
Matrix4 scaling(Vector3 vec)
{
	Matrix4 mat;


	mat = Matrix4(
		vec.x, 0, 0, 0,
		0, vec.y, 0, 0,
		0, 0, vec.z, 0,
		0, 0, 0, 1		
	);


	return mat;
}


// [TODO] given a float value then ouput a rotation matrix alone axis-X (rotate alone axis-X)
Matrix4 rotateX(GLfloat val)
{
	Matrix4 mat;


	mat = Matrix4(
		1, 0, 0, 0,
		0, cosf(val), -sinf(val), 0,
		0, sinf(val), cosf(val), 0,
		0, 0, 0, 1
	);


	return mat;
}

// [TODO] given a float value then ouput a rotation matrix alone axis-Y (rotate alone axis-Y)
Matrix4 rotateY(GLfloat val)
{
	Matrix4 mat;


	mat = Matrix4(
		cosf(val), 0, sinf(val), 0,
		0, 1, 0, 0,
		-sinf(val), 0, cosf(val), 0,
		0, 0, 0, 1
	);


	return mat;
}

// [TODO] given a float value then ouput a rotation matrix alone axis-Z (rotate alone axis-Z)
Matrix4 rotateZ(GLfloat val)
{
	Matrix4 mat;


	mat = Matrix4(
		cosf(val), -sinf(val), 0, 0,
		sinf(val), cos(val), 0, 0,
		0, 0, 1, 0,
		0, 0, 0, 1
	);


	return mat;
}

Matrix4 rotate(Vector3 vec)
{
	return rotateX(vec.x)*rotateY(vec.y)*rotateZ(vec.z);
}

// [TODO] compute viewing matrix accroding to the setting of main_camera
void setViewingMatrix()
{
	Matrix4 T = translate(-main_camera.position);
	Vector3 rx = (main_camera.center-main_camera.position).cross(main_camera.up_vector-main_camera.position);
	rx /= rx.length();
	Vector3 rz = (main_camera.center-main_camera.position)/((main_camera.center-main_camera.position).length());
	Vector3 ry = rx.cross(rz).normalize();
	view_matrix = Matrix4(rx.x, rx.y, rx.z, 0, ry.x, ry.y, ry.z, 0, -rz.x, -rz.y, -rz.z, 0, 0, 0, 0, 1) * T;
}

// [TODO] compute orthogonal projection matrix
void setOrthogonal()
{
	cur_proj_mode = Orthogonal;    
	project_matrix = Matrix4(
		2 / (proj.right - proj.left), 0, 0, -(proj.right + proj.left) / (proj.right - proj.left),
		0, 2 / (proj.top - proj.bottom), 0, -(proj.top + proj.bottom) / (proj.top - proj.bottom),
		0, 0, -2 / (proj.farClip - proj.nearClip), -(proj.farClip + proj.nearClip) / (proj.farClip - proj.nearClip),
		0, 0, 0, 1
	);
}

// [TODO] compute persepective projection matrix
void setPerspective()
{
	cur_proj_mode = Perspective;    
	float f = cos(proj.fovy / 2) / sin(proj.fovy / 2);
	project_matrix = Matrix4(
		-f / proj.aspect, 0, 0, 0,
		0, -f, 0, 0,
		0, 0, (proj.farClip + proj.nearClip) / (proj.nearClip - proj.farClip), (2.0 * proj.farClip * proj.nearClip) / (proj.nearClip - proj.farClip),
		0, 0, -1, 0
	);
}

void setVector3(string dest, Vector3 vec) {
    glUniform3f(glGetUniformLocation(ShaderID, dest.c_str()), vec.x, vec.y, vec.z);
}

void setVector3(string dest, float x, float y, float z) {
    glUniform3f(glGetUniformLocation(ShaderID, dest.c_str()), x, y, z);
}

void setfloat(string dest, float a) {
    glUniform1f(glGetUniformLocation(ShaderID, dest.c_str()), a);
}


// Vertex buffers
GLuint VAO, VBO;

// Call back function for window reshape
void ChangeSize(GLFWwindow* window, int width, int height)
{
	int	m;
	if(width < height * proj.aspect){
		m = width / proj.aspect;
	}else{
		m = height;
	}

	WINDOW_HEIGHT = m;
	WINDOW_WIDTH = m * proj.aspect;
	// [TODO] change your aspect ratio
}

// Render function for display rendering
void RenderScene(void) {	
	// clear canvas
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

	Matrix4 T, R, S, MVP, MV;
	// [TODO] update translation, rotation and scaling
	T = translate(models[cur_idx].position);
	R = rotate(models[cur_idx].rotation);
	S = scaling(models[cur_idx].scale);
	MVP = project_matrix * view_matrix * T * R * S;
	MV = view_matrix * T * R * S;
	GLfloat mvp[16];
	// [TODO] multiply all the matrix
	// [TODO] row-major ---> column-major
	mvp[0] = MVP[0];  mvp[4] = MVP[1];  mvp[8]  = MVP[2];  mvp[12] = MVP[3];
	mvp[1] = MVP[4];  mvp[5] = MVP[5];  mvp[9]  = MVP[6];  mvp[13] = MVP[7];
	mvp[2] = MVP[8];  mvp[6] = MVP[9];  mvp[10] = MVP[10]; mvp[14] = MVP[11];
	mvp[3] = MVP[12]; mvp[7] = MVP[13]; mvp[11] = MVP[14]; mvp[15] = MVP[15];

	// use uniform to send mvp to vertex shader
	glUniformMatrix4fv(glGetUniformLocation(ShaderID, "mv"), 1, GL_FALSE, MV.getTranspose());
	glUniformMatrix4fv(glGetUniformLocation(ShaderID, "view_matrix"), 1, GL_FALSE, view_matrix.getTranspose());
	glUniformMatrix4fv(glGetUniformLocation(ShaderID, "mvp"), 1, GL_FALSE, mvp);
	
	glUniform1i(glGetUniformLocation(ShaderID, "vertex_pixel"), 0);
	glViewport(0, 0, float(WINDOW_WIDTH)/2, WINDOW_HEIGHT);
	for (int i = 0; i < models[cur_idx].shapes.size(); i++) {
		setVector3("material.ambient", models[cur_idx].shapes[i].material.Ka);
		setVector3("material.diffuse", models[cur_idx].shapes[i].material.Kd);
		setVector3("material.specular", models[cur_idx].shapes[i].material.Ks);
		setfloat("material.shininess", lightAtt.shininess);

		glBindVertexArray(models[cur_idx].shapes[i].vao);
		glDrawArrays(GL_TRIANGLES, 0, models[cur_idx].shapes[i].vertex_count);

	}

	glUniform1i(glGetUniformLocation(ShaderID, "vertex_pixel"), 1);
	glViewport(float(WINDOW_WIDTH)/2, 0, float(WINDOW_WIDTH)/2, WINDOW_HEIGHT);
	for (int i = 0; i < models[cur_idx].shapes.size(); i++) 
	{
		setVector3("material.ambient", models[cur_idx].shapes[i].material.Ka);
		setVector3("material.diffuse", models[cur_idx].shapes[i].material.Kd);
		setVector3("material.specular", models[cur_idx].shapes[i].material.Ks);
		setfloat("material.shininess", lightAtt.shininess);

		glBindVertexArray(models[cur_idx].shapes[i].vao);
		glDrawArrays(GL_TRIANGLES, 0, models[cur_idx].shapes[i].vertex_count);
	}
}


void KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
	// [TODO] Call back function for keyboard
	if(key == GLFW_KEY_Z && action == GLFW_PRESS){
		cur_idx == 0 ? cur_idx = models.size()-1 : cur_idx--;
	}else if(key == GLFW_KEY_X && action == GLFW_PRESS){
		cur_idx = (cur_idx+1) % 5;
	}else if(key == GLFW_KEY_T && action == GLFW_PRESS){
		cur_trans_mode = GeoTranslation;
	}else if(key == GLFW_KEY_S && action == GLFW_PRESS){
		cur_trans_mode = GeoScaling;
	}else if(key == GLFW_KEY_R && action == GLFW_PRESS){
		cur_trans_mode = GeoRotation;
	}else if(key == GLFW_KEY_L && action == GLFW_PRESS){
		cur_light_idx = (cur_light_idx+1) % 3;
		glUniform1i(glGetUniformLocation(ShaderID, "lightmode"), cur_light_idx);
	}else if(key == GLFW_KEY_K && action == GLFW_PRESS){
		cur_trans_mode = LightEdit;
	}else if(key == GLFW_KEY_J && action == GLFW_PRESS){
		cur_trans_mode = ShininessEdit;
	}else if(key == GLFW_KEY_I && action == GLFW_PRESS){
		Matrix4 T, R, S;
		T = translate(models[cur_idx].position);
		R = rotate(models[cur_idx].rotation);
		S = scaling(models[cur_idx].scale);

		cout << "\nTranslation Matrix:\n";
		for(int i=0; i<4; i++){
			for(int j=0; j<4; j++){
				cout << T[i * 4 + j] << " ";
			}
			cout << "\n";
		}
		cout << "\nRotation Matrix:\n";
		for(int i=0; i<4; i++){
			for(int j=0; j<4; j++){
				cout << R[i * 4 + j] << " ";
			}
			cout << "\n";
		}
		cout << "\nScaling Matrix:\n";
		for(int i=0; i<4; i++){
			for(int j=0; j<4; j++){
				cout << S[i * 4 + j] << " ";
			}
			cout << "\n";
		}
		cout << "\nViewing Matrix:\n";
		for(int i=0; i<4; i++){
			for(int j=0; j<4; j++){
				cout << view_matrix[i * 4 + j] << " ";
			}
			cout << "\n";
		}
		cout << "\nProjection Matrix:\n";
		for(int i=0; i<4; i++){
			for(int j=0; j<4; j++){
				cout << project_matrix[i * 4 + j] << " ";
			}
			cout << "\n";
		}
	}
}

void scroll_callback(GLFWwindow* window, double xoffset, double yoffset)
{
	// [TODO] scroll up positive, otherwise it would be negtive
	switch(cur_trans_mode){
	  case GeoTranslation:
		models[cur_idx].position.z -= (yoffset * 0.03);
	  	break;
	  case GeoRotation:
	  	models[cur_idx].rotation.z -= (yoffset * 0.8 * 3.141592/180);
	  	break;
	  case GeoScaling:
	  	models[cur_idx].scale.z -= (yoffset * 0.03);
		break;
	  case ViewCenter:
	  	main_camera.center.z -= (yoffset * 0.03);
		setViewingMatrix();
		break;
	  case ViewEye:
	  	main_camera.position.z -= (yoffset * 0.03);
	  	setViewingMatrix();
		break;
	  case LightEdit:
	  	if (cur_light_idx == 0) {
			lightAtt.directional_diffuse -= Vector3(yoffset * 0.1, yoffset * 0.1, yoffset * 0.1);
			setVector3("directional.diffuse", lightAtt.directional_diffuse);
		} else if (cur_light_idx == 1) {
			lightAtt.point_diffuse -= Vector3(yoffset * 0.1, yoffset * 0.1, yoffset * 0.1);
			setVector3("point.diffuse", lightAtt.directional_diffuse);
		} else {
			lightAtt.spot_cutoff -= yoffset * 0.1;
			setfloat("spot.cutoff", lightAtt.spot_cutoff);
		}
	  	break;
	  case ShininessEdit:
		lightAtt.shininess = max(lightAtt.shininess - yoffset * 0.3, 0);
		setfloat("material.shininess", lightAtt.shininess);
	  	break;
 	  default:
	  	main_camera.up_vector.z -= (yoffset * 0.03);
		setViewingMatrix();
		break;
	}
}

void mouse_button_callback(GLFWwindow* window, int button, int action, int mods)
{
	// [TODO] mouse press callback function
	if (button == GLFW_MOUSE_BUTTON_LEFT) {
		if (action == GLFW_PRESS && mouse_pressed == false) {
			mouse_pressed = true;
			glfwGetCursorPos(window, &starting_press_x, &starting_press_y);
		} else if (action == GLFW_RELEASE) {
			mouse_pressed = false;
		}
	}
}

static void cursor_pos_callback(GLFWwindow* window, double xpos, double ypos)
{
	// [TODO] cursor position callback function
	if (mouse_pressed == true) {
		double x_offset = (xpos - starting_press_x) * 0.01;
		double y_offset = (ypos - starting_press_y) * 0.01;
		starting_press_x = xpos;
		starting_press_y = ypos;

		switch (cur_trans_mode) {
		case GeoTranslation:
			models[cur_idx].position.x += x_offset;
			models[cur_idx].position.y -= y_offset;
			break;
		case GeoRotation:
			models[cur_idx].rotation.x += y_offset * 3.141592/180 * 50;
			models[cur_idx].rotation.y -= x_offset * 3.141592/180 * 50;			
			break;
		case GeoScaling:
			models[cur_idx].scale.x += x_offset;
			models[cur_idx].scale.y -= y_offset;
			break;
		case LightEdit:
			lightAtt.directional_position -= Vector3(-x_offset, y_offset, 0);
			setVector3("directional.position", lightAtt.directional_position);
			lightAtt.point_position -= Vector3(-x_offset, y_offset, 0);
			setVector3("point.position", lightAtt.point_position);
			lightAtt.spot_position -= Vector3(-x_offset, y_offset, 0);
			setVector3("spot.position", lightAtt.spot_position);
			break;
		default:
			break;
		}
	}
}

void setShaders()
{
	GLuint v, f;
	char *vs = NULL;
	char *fs = NULL;

	v = glCreateShader(GL_VERTEX_SHADER);
	f = glCreateShader(GL_FRAGMENT_SHADER);

	vs = textFileRead("shader.vs");
	fs = textFileRead("shader.fs");

	glShaderSource(v, 1, (const GLchar**)&vs, NULL);
	glShaderSource(f, 1, (const GLchar**)&fs, NULL);

	free(vs);
	free(fs);

	GLint success;
	char infoLog[1000];
	// compile vertex shader
	glCompileShader(v);
	// check for shader compile errors
	glGetShaderiv(v, GL_COMPILE_STATUS, &success);
	if (!success)
	{
		glGetShaderInfoLog(v, 1000, NULL, infoLog);
		std::cout << "ERROR: VERTEX SHADER COMPILATION FAILED\n" << infoLog << std::endl;
	}

	// compile fragment shader
	glCompileShader(f);
	// check for shader compile errors
	glGetShaderiv(f, GL_COMPILE_STATUS, &success);
	if (!success)
	{
		glGetShaderInfoLog(f, 1000, NULL, infoLog);
		std::cout << "ERROR: FRAGMENT SHADER COMPILATION FAILED\n" << infoLog << std::endl;
	}

	// create program object
	ShaderID = glCreateProgram();

	// attach shaders to program object
	glAttachShader(ShaderID, f);
	glAttachShader(ShaderID, v);

	// link program
	glLinkProgram(ShaderID);
	// check for linking errors
	glGetProgramiv(ShaderID, GL_LINK_STATUS, &success);
	if (!success) {
		glGetProgramInfoLog(ShaderID, 1000, NULL, infoLog);
		std::cout << "ERROR: SHADER PROGRAM LINKING FAILED\n" << infoLog << std::endl;
	}

	glDeleteShader(v);
	glDeleteShader(f);

	if (success)
		glUseProgram(ShaderID);
    else
    {
        system("pause");
        exit(123);
    }

	setVector3("directional.position", 1, 1, 1);
	setVector3("directional.direction", -1, -1, -1);
	setVector3("directional.diffuse", 1, 1, 1);
	setVector3("directional.ambient", 0.15, 0.15, 0.15);

	setVector3("point.position", 0, 2, 1);
	setVector3("point.diffuse", 1, 1, 1);
	setVector3("point.ambient", 0.15, 0.15, 0.15);
	setfloat("point.constant", 0.01);
	setfloat("point.linear", 0.8);
	setfloat("point.quadratic", 0.1);

	setVector3("spot.position", 0, 0, 2);
	setVector3("spot.direction", 0, 0, -1);
	setVector3("spot.diffuse", 1, 1, 1);
	setVector3("spot.ambient", 0.15, 0.15, 0.15);
	setfloat("spot.exponent", 50);
	setfloat("spot.cutoff", 30);
	setfloat("spot.constant", 0.05);
	setfloat("spot.linear", 0.3);
	setfloat("spot.quadratic", 0.6);
	glUniform1i(glGetUniformLocation(ShaderID, "lightmode"), cur_light_idx);
}

void normalization(tinyobj::attrib_t* attrib, vector<GLfloat>& vertices, vector<GLfloat>& colors, vector<GLfloat>& normals, tinyobj::shape_t* shape)
{
	vector<float> xVector, yVector, zVector;
	float minX = 10000, maxX = -10000, minY = 10000, maxY = -10000, minZ = 10000, maxZ = -10000;

	// find out min and max value of X, Y and Z axis
	for (int i = 0; i < attrib->vertices.size(); i++)
	{
		//maxs = max(maxs, attrib->vertices.at(i));
		if (i % 3 == 0)
		{

			xVector.push_back(attrib->vertices.at(i));

			if (attrib->vertices.at(i) < minX)
			{
				minX = attrib->vertices.at(i);
			}

			if (attrib->vertices.at(i) > maxX)
			{
				maxX = attrib->vertices.at(i);
			}
		}
		else if (i % 3 == 1)
		{
			yVector.push_back(attrib->vertices.at(i));

			if (attrib->vertices.at(i) < minY)
			{
				minY = attrib->vertices.at(i);
			}

			if (attrib->vertices.at(i) > maxY)
			{
				maxY = attrib->vertices.at(i);
			}
		}
		else if (i % 3 == 2)
		{
			zVector.push_back(attrib->vertices.at(i));

			if (attrib->vertices.at(i) < minZ)
			{
				minZ = attrib->vertices.at(i);
			}

			if (attrib->vertices.at(i) > maxZ)
			{
				maxZ = attrib->vertices.at(i);
			}
		}
	}

	float offsetX = (maxX + minX) / 2;
	float offsetY = (maxY + minY) / 2;
	float offsetZ = (maxZ + minZ) / 2;

	for (int i = 0; i < attrib->vertices.size(); i++)
	{
		if (offsetX != 0 && i % 3 == 0)
		{
			attrib->vertices.at(i) = attrib->vertices.at(i) - offsetX;
		}
		else if (offsetY != 0 && i % 3 == 1)
		{
			attrib->vertices.at(i) = attrib->vertices.at(i) - offsetY;
		}
		else if (offsetZ != 0 && i % 3 == 2)
		{
			attrib->vertices.at(i) = attrib->vertices.at(i) - offsetZ;
		}
	}

	float greatestAxis = maxX - minX;
	float distanceOfYAxis = maxY - minY;
	float distanceOfZAxis = maxZ - minZ;

	if (distanceOfYAxis > greatestAxis)
	{
		greatestAxis = distanceOfYAxis;
	}

	if (distanceOfZAxis > greatestAxis)
	{
		greatestAxis = distanceOfZAxis;
	}

	float scale = greatestAxis / 2;

	for (int i = 0; i < attrib->vertices.size(); i++)
	{
		//std::cout << i << " = " << (double)(attrib.vertices.at(i) / greatestAxis) << std::endl;
		attrib->vertices.at(i) = attrib->vertices.at(i) / scale;
	}
	size_t index_offset = 0;
	for (size_t f = 0; f < shape->mesh.num_face_vertices.size(); f++) {
		int fv = shape->mesh.num_face_vertices[f];

		// Loop over vertices in the face.
		for (size_t v = 0; v < fv; v++) {
			// access to vertex
			tinyobj::index_t idx = shape->mesh.indices[index_offset + v];
			vertices.push_back(attrib->vertices[3 * idx.vertex_index + 0]);
			vertices.push_back(attrib->vertices[3 * idx.vertex_index + 1]);
			vertices.push_back(attrib->vertices[3 * idx.vertex_index + 2]);
			// Optional: vertex colors
			colors.push_back(attrib->colors[3 * idx.vertex_index + 0]);
			colors.push_back(attrib->colors[3 * idx.vertex_index + 1]);
			colors.push_back(attrib->colors[3 * idx.vertex_index + 2]);
			// Optional: vertex normals
			if (idx.normal_index >= 0) {
				normals.push_back(attrib->normals[3 * idx.normal_index + 0]);
				normals.push_back(attrib->normals[3 * idx.normal_index + 1]);
				normals.push_back(attrib->normals[3 * idx.normal_index + 2]);
			}
		}
		index_offset += fv;
	}
}

string GetBaseDir(const string& filepath) {
	if (filepath.find_last_of("/\\") != std::string::npos)
		return filepath.substr(0, filepath.find_last_of("/\\"));
	return "";
}

void LoadModels(string model_path)
{
	vector<tinyobj::shape_t> shapes;
	vector<tinyobj::material_t> materials;
	tinyobj::attrib_t attrib;
	vector<GLfloat> vertices;
	vector<GLfloat> colors;
	vector<GLfloat> normals;

	string err;
	string warn;

	string base_dir = GetBaseDir(model_path); // handle .mtl with relative path

#ifdef _WIN32
	base_dir += "\\";
#else
	base_dir += "/";
#endif

	bool ret = tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, model_path.c_str(), base_dir.c_str());

	if (!warn.empty()) {
		cout << warn << std::endl;
	}

	if (!err.empty()) {
		cerr << err << std::endl;
	}

	if (!ret) {
		exit(1);
	}

	printf("Load Models Success ! Shapes size %d Material size %d\n", shapes.size(), materials.size());
	model tmp_model;

	vector<PhongMaterial> allMaterial;
	for (int i = 0; i < materials.size(); i++)
	{
		PhongMaterial material;
		material.Ka = Vector3(materials[i].ambient[0], materials[i].ambient[1], materials[i].ambient[2]);
		material.Kd = Vector3(materials[i].diffuse[0], materials[i].diffuse[1], materials[i].diffuse[2]);
		material.Ks = Vector3(materials[i].specular[0], materials[i].specular[1], materials[i].specular[2]);
		allMaterial.push_back(material);
	}

	for (int i = 0; i < shapes.size(); i++)
	{

		vertices.clear();
		colors.clear();
		normals.clear();
		normalization(&attrib, vertices, colors, normals, &shapes[i]);
		// printf("Vertices size: %d", vertices.size() / 3);

		Shape tmp_shape;
		glGenVertexArrays(1, &tmp_shape.vao);
		glBindVertexArray(tmp_shape.vao);

		glGenBuffers(1, &tmp_shape.vbo);
		glBindBuffer(GL_ARRAY_BUFFER, tmp_shape.vbo);
		glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(GL_FLOAT), &vertices.at(0), GL_STATIC_DRAW);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, 0);
		tmp_shape.vertex_count = vertices.size() / 3;

		glGenBuffers(1, &tmp_shape.p_color);
		glBindBuffer(GL_ARRAY_BUFFER, tmp_shape.p_color);
		glBufferData(GL_ARRAY_BUFFER, colors.size() * sizeof(GL_FLOAT), &colors.at(0), GL_STATIC_DRAW);
		glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 0, 0);

		glGenBuffers(1, &tmp_shape.p_normal);
		glBindBuffer(GL_ARRAY_BUFFER, tmp_shape.p_normal);
		glBufferData(GL_ARRAY_BUFFER, normals.size() * sizeof(GL_FLOAT), &normals.at(0), GL_STATIC_DRAW);
		glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 0, 0);

		glEnableVertexAttribArray(0);
		glEnableVertexAttribArray(1);
		glEnableVertexAttribArray(2);

		// not support per face material, use material of first face
		if (allMaterial.size() > 0)
			tmp_shape.material = allMaterial[shapes[i].mesh.material_ids[0]];
		tmp_model.shapes.push_back(tmp_shape);
	}
	shapes.clear();
	materials.clear();
	models.push_back(tmp_model);
}

void initParameter()
{
	proj.left = -1;
	proj.right = 1;
	proj.top = 1;
	proj.bottom = -1;
	proj.nearClip = 0.001;
	proj.farClip = 100.0;
	proj.fovy = 80;
	proj.aspect = (float)WINDOW_WIDTH / (float)WINDOW_HEIGHT;

	main_camera.position = Vector3(0.0f, 0.0f, 2.0f);
	main_camera.center = Vector3(0.0f, 0.0f, 0.0f);
	main_camera.up_vector = Vector3(0.0f, 1.0f, 0.0f);

	lightAtt.directional_position = Vector3(1, 1, 1);
	lightAtt.directional_diffuse = Vector3(1, 1, 1);
	lightAtt.point_position = Vector3(0, 2, 1);
	lightAtt.point_diffuse = Vector3(1, 1, 1);
	lightAtt.spot_position = Vector3(0, 0, 2);
	lightAtt.spot_cutoff = 30;
	lightAtt.shininess = 64;
	
	setViewingMatrix();
	setPerspective();	//set default projection matrix as perspective matrix
}

void setupRC()
{
	// setup shaders
	setShaders();
	initParameter();

	// OpenGL States and Values
	glClearColor(0.2, 0.2, 0.2, 1.0);
	vector<string> model_list{ "../NormalModels/bunny5KN.obj", "../NormalModels/dragon10KN.obj", "../NormalModels/lucy25KN.obj", "../NormalModels/teapot4KN.obj", "../NormalModels/dolphinN.obj"};
	// [TODO] Load five model at here
	for(int i = 0; i < model_list.size(); i++){
		LoadModels(model_list[i]);
	}
}

void glPrintContextInfo(bool printExtension)
{
	cout << "GL_VENDOR = " << (const char*)glGetString(GL_VENDOR) << endl;
	cout << "GL_RENDERER = " << (const char*)glGetString(GL_RENDERER) << endl;
	cout << "GL_VERSION = " << (const char*)glGetString(GL_VERSION) << endl;
	cout << "GL_SHADING_LANGUAGE_VERSION = " << (const char*)glGetString(GL_SHADING_LANGUAGE_VERSION) << endl;
	if (printExtension)
	{
		GLint numExt;
		glGetIntegerv(GL_NUM_EXTENSIONS, &numExt);
		cout << "GL_EXTENSIONS =" << endl;
		for (GLint i = 0; i < numExt; i++)
		{
			cout << "\t" << (const char*)glGetStringi(GL_EXTENSIONS, i) << endl;
		}
	}
}


int main(int argc, char **argv)
{
    // initial glfw
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    
#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE); // fix compilation on OS X
#endif

    
    // create window
	GLFWwindow* window = glfwCreateWindow(WINDOW_WIDTH, WINDOW_HEIGHT, "Student ID HW2", NULL, NULL);
    if (window == NULL)
    {
        std::cout << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);
    
    
    // load OpenGL function pointer
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        std::cout << "Failed to initialize GLAD" << std::endl;
        return -1;
    }
    
	// register glfw callback functions
    glfwSetKeyCallback(window, KeyCallback);
	glfwSetScrollCallback(window, scroll_callback);
	glfwSetMouseButtonCallback(window, mouse_button_callback);
	glfwSetCursorPosCallback(window, cursor_pos_callback);

    glfwSetFramebufferSizeCallback(window, ChangeSize);
	glEnable(GL_DEPTH_TEST);
	// Setup render context
	setupRC();

	// main loop
    while (!glfwWindowShouldClose(window))
    {
        // render
        RenderScene();
        
        // swap buffer from back to front
        glfwSwapBuffers(window);
        
        // Poll input event
        glfwPollEvents();
    }
	
	// just for compatibiliy purposes
	return 0;
}
