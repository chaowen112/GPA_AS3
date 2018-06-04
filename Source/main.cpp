#include "../Externals/Include/Include.h"
#include <vector>
#define MENU_TIMER_START 1
#define MENU_TIMER_STOP 2
#define MENU_EXIT 3
#define MENU_BAR_SWITCH 4
#define MENU_INDEX_BLUR 5
#define MENU_INDEX_RED_BLUE 6
#define MENU_INDEX_LAPLACIAN 7
#define MENU_INDEX_SHARP 8
#define PI 3.1415926525

GLubyte timer_cnt = 0;
bool timer_enabled = true;
unsigned int timer_speed = 20;

using namespace glm;
using namespace std;

mat4 view;
mat4 projection;

double last_x;
double last_y;
double last_z;

GLint um4p;
GLint um4mv;
GLuint  program;
GLuint frameProgram;
GLuint window_vao;
GLuint window_buffer;
GLint barSwitch;
GLint filterIndex;
GLint iResolution;
GLint enlargeRate;
GLint iTime;

// FBO parameter
GLuint            FBO;
GLuint            depthRBO;
GLuint            FBODataTexture;
GLuint channel;

double pan = 0, tilt = 0;

bool bar = false;
int filterIndexValue = 0;
float iResolutionValue[2] = {600,600};
float enlargeRateValue = 2.0;
float iTimeValue = 1;

vec3 eye(0.0f, 0.0f, 0.0f);
vec3 ref_pos(0.0,100.0,0.0);
vec3 up(0.0,1.0,0.0);
vec3 offset(0.0f,0.0f,0.0f);

static const GLfloat window_positions[] =
{
    1.0f,-1.0f,1.0f,0.0f,
    -1.0f,-1.0f,0.0f,0.0f,
    -1.0f,1.0f,0.0f,1.0f,
    1.0f,1.0f,1.0f,1.0f
};

char** loadShaderSource(const char* file)
{
    FILE* fp = fopen(file, "rb");
    fseek(fp, 0, SEEK_END);
    long sz = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    char *src = new char[sz + 1];
    fread(src, sizeof(char), sz, fp);
    src[sz] = '\0';
    char **srcp = new char*[1];
    srcp[0] = src;
    return srcp;
}

void My_Reshape(int width, int height);

void freeShaderSource(char** srcp)
{
    delete[] srcp[0];
    delete[] srcp;
}

// define a simple data structure for storing texture image raw data
typedef struct _TextureData
{
    _TextureData(void) :
        width(0),
        height(0),
        data(0)
    {
    }

    int width;
    int height;
    unsigned char* data;
} TextureData;

struct Shape
{
    GLuint vao;
    GLuint vbo_position;
    GLuint vbo_normal;
    GLuint vbo_texcoord;
    GLuint ibo;
    int drawCount;
    int materialId;
};

std::vector<Shape> shapes;

struct Material
{
    GLuint diffuse_tex;
};

std::vector<Material> materials;

// load a png image and return a TextureData structure with raw data
// not limited to png format. works with any image format that is RGBA-32bit
TextureData loadPNG(const char* const pngFilepath)
{
    TextureData texture;
    int components;

    // load the texture with stb image, force RGBA (4 components required)
    stbi_uc *data = stbi_load(pngFilepath, &texture.width, &texture.height, &components, 4);

    // is the image successfully loaded?
    if (data != NULL)
    {
        // copy the raw data
        size_t dataSize = texture.width * texture.height * 4 * sizeof(unsigned char);
        texture.data = new unsigned char[dataSize];
        memcpy(texture.data, data, dataSize);

        // mirror the image vertically to comply with OpenGL convention
        for (size_t i = 0; i < texture.width; ++i)
        {
            for (size_t j = 0; j < texture.height / 2; ++j)
            {
                for (size_t k = 0; k < 4; ++k)
                {
                    size_t coord1 = (j * texture.width + i) * 4 + k;
                    size_t coord2 = ((texture.height - j - 1) * texture.width + i) * 4 + k;
                    std::swap(texture.data[coord1], texture.data[coord2]);
                }
            }
        }

        // release the loaded image
        stbi_image_free(data);
    }

    return texture;
}

void MyLoadScene(char *filePath)
{
    const aiScene *scene = aiImportFile(filePath, aiProcessPreset_TargetRealtime_MaxQuality);
    TextureData data;
    for (unsigned int i = 0; i < scene->mNumMaterials; ++i)
    {
        aiMaterial *material = scene->mMaterials[i];
        Material Material;
        aiString texturePath;
        if (material->GetTexture(aiTextureType_DIFFUSE, 0, &texturePath) ==
            aiReturn_SUCCESS)
        {
            // load width, height and data from texturePath.C_Str();
            data = loadPNG(texturePath.C_Str());
            glGenTextures(1, &Material.diffuse_tex);
            glBindTexture(GL_TEXTURE_2D, Material.diffuse_tex);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, data.width, data.height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data.data);
            glGenerateMipmap(GL_TEXTURE_2D);
        } else {
            // load some default image as default_diffuse_tex
            //Material.diffuse_tex = default_diffuse_tex;
            printf("gentexture fail!!  texturePath = %s\n", texturePath.C_Str());
        }
        // save material...
        materials.push_back(Material);
    }
    for (unsigned int i = 0; i < scene->mNumMeshes; ++i)
    {
        aiMesh *mesh = scene->mMeshes[i];
        Shape shape;
        glGenVertexArrays(1, &shape.vao);
        glBindVertexArray(shape.vao);
        // create 3 vbos to hold data
        glGenBuffers(1, &shape.vbo_position);
        glGenBuffers(1, &shape.vbo_normal);
        glGenBuffers(1, &shape.vbo_texcoord);
        float position[mesh->mNumVertices*3];// = new float[mesh->mNumVertices*3];
        float normals[mesh->mNumVertices*3];// = new float[mesh->mNumVertices*3];
        float texture[mesh->mNumVertices*2];// = new float[mesh->mNumVertices*2];
        for (unsigned int v = 0; v < mesh->mNumVertices; ++v)
        {
            position[3*v] = mesh->mVertices[v][0];
            position[3*v+1] = mesh->mVertices[v][1];
            position[3*v+2] = mesh->mVertices[v][2];
            normals[3*v] = mesh->mVertices[v][0];
            normals[3*v+1] = mesh->mVertices[v][1];
            normals[3*v+2] = mesh->mVertices[v][2];
            texture[2*v] = mesh->mTextureCoords[0][v][0];
            texture[2*v+1] = mesh->mTextureCoords[0][v][1];
            //mesh->mVertices[v][0~2] => position;
            // mesh->mNormals[v][0~2] => normal
            // mesh->mTextureCoords[0][v][0~1] => texcoord
        }
        glBindBuffer(GL_ARRAY_BUFFER, shape.vbo_position);
        glBufferData(GL_ARRAY_BUFFER, 3*sizeof(float)*mesh->mNumVertices, position, GL_STATIC_DRAW);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, 0);
        glBindBuffer(GL_ARRAY_BUFFER, shape.vbo_texcoord);
        glBufferData(GL_ARRAY_BUFFER, 2*sizeof(float)*mesh->mNumVertices, texture, GL_STATIC_DRAW);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 0, 0);
        glBindBuffer(GL_ARRAY_BUFFER, shape.vbo_normal);
        glBufferData(GL_ARRAY_BUFFER, 3*sizeof(float)*mesh->mNumVertices, normals, GL_STATIC_DRAW);
        glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 0, 0);
        // create 1 ibo to hold data
        glGenBuffers(1, &shape.ibo);
        //unsigned int *index = new unsigned int[mesh->mNumFaces*3];
        int index[mesh->mNumFaces*3];
        for (unsigned int f = 0; f < mesh->mNumFaces; ++f)
        {
            index[3*f] = mesh->mFaces[f].mIndices[0];
            index[3*f+1]= mesh->mFaces[f].mIndices[1];
            index[3*f+2] = mesh->mFaces[f].mIndices[2];
            // mesh->mFaces[f].mIndices[0~2] => index
        }
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, shape.ibo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, 3*sizeof(int)*mesh->mNumFaces, index, GL_STATIC_DRAW);
        // glVertexAttribPointer / glEnableVertexArray calls...
        //delete index;
        shape.materialId = mesh->mMaterialIndex;
        shape.drawCount = mesh->mNumFaces * 3;
        // save shape...
        shapes.push_back(shape);
    }
    aiReleaseImport(scene);
}

void My_Init()
{
    glClearColor(1.0f, 0.6f, 0.0f, 1.0f);
	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LEQUAL);
    printf("draw framebuffer: %x\n", GL_DRAW_FRAMEBUFFER);
    printf("framebuffer: %x\n", GL_FRAMEBUFFER);
    
    program = glCreateProgram();
    GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
    GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    char** vertexShaderSource = loadShaderSource("vertex.vs.glsl");
    char** fragmentShaderSource = loadShaderSource("fragment.fs.glsl");
    glShaderSource(vertexShader, 1, vertexShaderSource, NULL);
    glShaderSource(fragmentShader, 1, fragmentShaderSource, NULL);
    freeShaderSource(vertexShaderSource);
    freeShaderSource(fragmentShaderSource);
    glCompileShader(vertexShader);
    glCompileShader(fragmentShader);
    shaderLog(vertexShader);
    shaderLog(fragmentShader);
    glAttachShader(program, vertexShader);
    glAttachShader(program, fragmentShader);
    glLinkProgram(program);
    um4p = glGetUniformLocation(program, "um4p");
    um4mv = glGetUniformLocation(program, "um4mv");
    glUseProgram(program);
    MyLoadScene("sponza.obj");
    
    frameProgram = glCreateProgram();
    
    GLuint frameVertexShader = glCreateShader(GL_VERTEX_SHADER);
    GLuint frameFragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    char** frameVertexShaderSource = loadShaderSource("frame_vs.glsl");
    char** frameFragmentShaderSource = loadShaderSource("frame_fs.glsl");
    glShaderSource(frameVertexShader, 1, frameVertexShaderSource, NULL);
    glShaderSource(frameFragmentShader, 1, frameFragmentShaderSource, NULL);
    glCompileShader(frameVertexShader);
    glCompileShader(frameFragmentShader);
    shaderLog(frameVertexShader);
    shaderLog(frameFragmentShader);
    
    glAttachShader(frameProgram, frameVertexShader);
    glAttachShader(frameProgram, frameFragmentShader);
    glLinkProgram(frameProgram);
    barSwitch = glGetUniformLocation(frameProgram, "barSwitch");
    filterIndex = glGetUniformLocation(frameProgram, "filterIndex");
    iResolution = glGetUniformLocation(frameProgram, "iResolution");
    enlargeRate = glGetUniformLocation(frameProgram, "enlargeRate");
    iTime = glGetUniformLocation(frameProgram, "iTime");
    
    
    glGenVertexArrays(1, &window_vao);
    glBindVertexArray(window_vao);
    
    glGenBuffers(1, &window_buffer);
    glBindBuffer(GL_ARRAY_BUFFER, window_buffer);
    glBufferData(GL_ARRAY_BUFFER, sizeof(window_positions), window_positions, GL_STATIC_DRAW);
    
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(GL_FLOAT) * 4, 0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(GL_FLOAT) * 4, (const GLvoid*)(sizeof(GL_FLOAT) * 2));
    
    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);
    
    glGenFramebuffers(1, &FBO);
    
    ref_pos = vec3(100*cos(pan*PI/180), tilt, 100*sin(pan*PI/180));
    My_Reshape(600, 600);
}

void My_Display()
{
    // (1) Bind the framebuffer object correctly
    // (2) Draw the buffer with color
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, FBO);
    glDrawBuffer(GL_COLOR_ATTACHMENT0);
    
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glUseProgram(program);
    
    view = lookAt(eye+offset, ref_pos+offset, up);

    static const GLfloat white[] = { 1.0f, 1.0f, 1.0f, 1.0f };
    static const GLfloat one = 1.0f;
    
    // TODO :
    // (1) Clear the color buffer (GL_COLOR) with the color of white
    // (2) Clear the depth buffer (GL_DEPTH) with value one
    glClearBufferfv(GL_COLOR, 0, white);
    glClearBufferfv(GL_DEPTH, 0, &one);
    
    // Draw Scene
    glUniformMatrix4fv(um4mv, 1, GL_FALSE, value_ptr(view));
    glUniformMatrix4fv(um4p, 1, GL_FALSE, value_ptr(projection));
    glActiveTexture(GL_TEXTURE0);
    //glUniform1i(tex_location, 0);
    for(int i = 0; i < shapes.size(); ++i)
    {
        glBindVertexArray(shapes[i].vao);
        glEnableVertexAttribArray(0);
        glEnableVertexAttribArray(1);
        glEnableVertexAttribArray(2);
        int materialID = shapes[i].materialId;
        glBindTexture(GL_TEXTURE_2D, materials[materialID].diffuse_tex);
        glActiveTexture(GL_TEXTURE0);
        glDrawElements(GL_TRIANGLES, shapes[i].drawCount, GL_UNSIGNED_INT, 0);
    }
    
    // Re-bind the framebuffer and clear it
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glClearColor(1.0f, 0.0f, 0.0f, 1.0f);
    
    // setup barSwitch and index
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, FBODataTexture);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, channel);
    
    // TODO :
    // (1) Bind the vao we want to render
    // (2) Use the correct shader program
    glBindVertexArray(window_vao);
    glUseProgram(frameProgram);
    glUniform1i(barSwitch, bar);
    glUniform1i(filterIndex, filterIndexValue);
    glUniform1f(enlargeRate, enlargeRateValue);
    glUniform2fv(iResolution, 1,iResolutionValue);
    glUniform1f(iTime, iTimeValue);
    glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
    iTimeValue += 0.01;
    
    glutSwapBuffers();
}

void My_Reshape(int width, int height)
{
    iResolutionValue[0] = width;
    iResolutionValue[1] = height;
	glViewport(0, 0, width, height);
    float viewportAspect = (float)width / (float)height;
    projection = perspective(radians(60.0f), viewportAspect, 0.1f, 1000.0f);
    view = lookAt(eye+offset, ref_pos+offset, up);
    
    // If the windows is reshaped, we need to reset some settings of framebuffer
    glDeleteRenderbuffers(1, &depthRBO);
    glDeleteTextures(1, &FBODataTexture);
    glGenRenderbuffers(1, &depthRBO);
    glBindRenderbuffer(GL_RENDERBUFFER, depthRBO);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT32, width, height);
    
    // (1) Generate a texture for FBO
    // (2) Bind it so that we can specify the format of the textrue
    glGenTextures(1, &FBODataTexture);
    glBindTexture(GL_TEXTURE_2D, FBODataTexture);
    
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glGenTextures(1, &channel);
    glBindTexture(GL_TEXTURE_2D, channel);
    TextureData data;
    data = loadPNG("download.jpeg");
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, data.width, data.height, 0,GL_RGB, GL_UNSIGNED_BYTE, data.data);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    
    // (1) Bind the framebuffer with first parameter "GL_DRAW_FRAMEBUFFER"
    // (2) Attach a renderbuffer object to a framebuffer object
    // (3) Attach a texture image to a framebuffer object
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, FBO);
    glFramebufferRenderbuffer(GL_DRAW_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depthRBO);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, FBODataTexture, 0);
    
    glUniform2fv(iResolution, 1, value_ptr(vec2(600, 600)));
}

void My_Timer(int val)
{
	glutPostRedisplay();
	glutTimerFunc(timer_speed, My_Timer, val);
}

void My_Mouse(int button, int state, int x, int y)
{
    if(button == 6){
        printf("wheel up\n");
        return;
    }
	if(state == GLUT_DOWN)
	{
		printf("Mouse %d is pressed at (%d, %d)\n", button, x, y);
        last_x = x;
        last_y = y;
	}
	else if(state == GLUT_UP)
	{
		printf("Mouse %d is released at (%d, %d)\n", button, x, y);
        last_x = x;
        last_y = y;
	}
    if(button == GLUT_MIDDLE_BUTTON){
        enlargeRateValue += 0.2;
    }
    if(button == GLUT_LEFT_BUTTON && enlargeRateValue > 0.2){
        enlargeRateValue -= 0.2;
    }
}

vec3 normalize(vec3 v)
{
    float xxyyzz = v.x*v.x + v.y*v.y + v.z*v.z;
    
    float invLength = 1.0f/sqrtf(xxyyzz);
    v.x*=invLength;
    v.y*=invLength;
    v.z*=invLength;
    
    return v;
}

vec3 cross(vec3 a, vec3 b)
{
    return vec3(a.y*b.z - a.z*b.y, a.z*b.x - a.x * b.z, a.x*b.y - a.y * b.x );
}

void My_Keyboard(unsigned char key, int x, int y)
{
    vec3 goback = normalize(ref_pos - eye);
    vec3 goright = normalize(cross(goback,up));
    vec3 goup = normalize(cross(goback, goright));
    printf("Key %c is pressed at (%d, %d)\n", key, x, y);
    switch(key)
    {
        case 'z':
            offset += goup;
            break;
        case 'x':
            offset -= goup;
            break;
        case 'w':
            offset += goback;
            break;
        case 's':
            offset -= goback;
            break;
        case 'd':
            offset += goright;
            break;
        case 'a':
            offset -= goright;
            break;
    }
}

void My_SpecialKeys(int key, int x, int y)
{
    switch(key)
    {
        case GLUT_KEY_F1:
            printf("F1 is pressed at (%d, %d)\n", x, y);
            break;
        case GLUT_KEY_UP:
            printf("Page up is pressed at (%d, %d)\n", x, y);
            ref_pos += vec3(1.0f, 0.0f, 0.0f);
            eye += vec3(1.0f, 0.0f, 0.0f);
            break;
        case GLUT_KEY_DOWN:
            printf("Page up is pressed at (%d, %d)\n", x, y);
            ref_pos += vec3(-1.0f, 0.0f, 0.0f);
            eye += vec3(-1.0f, 0.0f, 0.0f);
            break;
        case GLUT_KEY_RIGHT:
            printf("Page up is pressed at (%d, %d)\n", x, y);
            ref_pos += vec3(0.0f, 1.0f, 0.0f);
            eye += vec3(0.0f, 1.0f, 0.0f);
            break;
        case GLUT_KEY_LEFT:
            printf("Left arrow is pressed at (%d, %d)\n", x, y);
            ref_pos += vec3(0.0f, -1.0f, 0.0f);
            eye += vec3(0.0f, -1.0f, 0.0f);
            break;
        default:
            printf("Other special key is pressed at (%d, %d)\n", x, y);
            break;
    }
}

void My_Menu(int id)
{
	switch(id)
	{
        case MENU_TIMER_START:
            if(!timer_enabled)
            {
                timer_enabled = true;
                glutTimerFunc(timer_speed, My_Timer, 0);
            }
            break;
        case MENU_TIMER_STOP:
            timer_enabled = false;
            break;
        case MENU_BAR_SWITCH:
            bar = !bar;
            printf("bar = !bar\n");
            break;
        case MENU_INDEX_BLUR:
            filterIndexValue = 0;
            printf("filterIndexValue = 0\n");
            break;
        case MENU_INDEX_RED_BLUE:
            filterIndexValue = 1;
            printf("filterIndexValue = 1\n");
            break;
        case MENU_INDEX_LAPLACIAN:
            filterIndexValue = 2;
            printf("filterIndexValue = 2\n");
            break;
        case MENU_INDEX_SHARP:
            filterIndexValue = 3;
            printf("filterIndexValue = 3\n");
            break;
        case 9:
            filterIndexValue = 4;
            printf("filterIndexValue = 4\n");
            break;
        case 10:
            filterIndexValue = 5;
            printf("filterIndexValue = 5\n");
            break;
        case 11:
            filterIndexValue = 6;
            printf("filterIndexValue = 6\n");
            break;
        case 12:
            filterIndexValue = 7;
            printf("filterIndexValue = 7\n");
            break;
        case 13:
            filterIndexValue = 8;
            printf("filterIndexValue = 8\n");
            break;
        case 14:
            filterIndexValue = 9;
            printf("filterIndexValue = 9\n");
            break;
        case 15:
            filterIndexValue = 10;
            printf("filterIndexValue = 10\n");
            break;
        case 16:
            filterIndexValue = 11;
            printf("filterIndexValue = 11\n");
            break;
        case 17:
            filterIndexValue = 12;
            printf("filterIndexValue = 12\n");
            break;
        case 18:
            filterIndexValue = 13;
            printf("filterIndexValue = 12\n");
            break;
        case 999:
            filterIndexValue = 999;
            printf("filterIndexValue = 999\n");
            break;
        case MENU_EXIT:
            exit(0);
            break;
        default:
            break;
	}
}

void My_Motion(int x, int y){
    //eye = vec3(30*cos(last_x_axis), last_y_axis, 20*sin(last_x_axis));
    pan -= 0.3*(x-last_x);
    tilt += 0.3*(y-last_y);
    ref_pos = vec3(100*cos(pan*PI/180), tilt, 100*sin(pan*PI/180));
    last_x = x;
    last_y = y;
    printf("%lf, %lf, %lf, %lf, %lf\n", ref_pos.x, ref_pos.y, ref_pos.z, pan, tilt);
    //up = vec3(0, sin(last_z_axis), cos(last_y_axis));
}

int main(int argc, char *argv[])
{
#ifdef __APPLE__
    // Change working directory to source code path
    chdir(__FILEPATH__("/../Assets/"));
#endif
	// Initialize GLUT and GLEW, then create a window.
	////////////////////
	glutInit(&argc, argv);
#ifdef _MSC_VER
	glutInitDisplayMode(GLUT_RGBA | GLUT_DOUBLE | GLUT_DEPTH);
#else
    glutInitDisplayMode(GLUT_3_2_CORE_PROFILE | GLUT_RGBA | GLUT_DOUBLE | GLUT_DEPTH);
#endif
	glutInitWindowPosition(100, 100);
	glutInitWindowSize(600, 600);
	glutCreateWindow("AS2_Framework"); // You cannot use OpenGL functions before this line; The OpenGL context must be created first by glutCreateWindow()!
#ifdef _MSC_VER
	glewInit();
#endif
    glPrintContextInfo();
    glewInit();
	My_Init();

	// Create a menu and bind it to mouse right button.
	int menu_main = glutCreateMenu(My_Menu);
	int menu_timer = glutCreateMenu(My_Menu);
    int menu_bar = glutCreateMenu(My_Menu);
    int menu_index = glutCreateMenu(My_Menu);

	glutSetMenu(menu_main);
	//glutAddSubMenu("Timer", menu_timer);
    glutAddMenuEntry("Comparation bar", MENU_BAR_SWITCH);
    glutAddSubMenu("Filter", menu_index);
	glutAddMenuEntry("Exit", MENU_EXIT);

	//glutSetMenu(menu_timer);
	//glutAddMenuEntry("Start", MENU_TIMER_START);
	//glutAddMenuEntry("Stop", MENU_TIMER_STOP);
    
    glutSetMenu(menu_index);
    glutAddMenuEntry("blur + quantization + DOG", MENU_INDEX_BLUR);
    glutAddMenuEntry("red blue stereo", MENU_INDEX_RED_BLUE);
    glutAddMenuEntry("laplacian", MENU_INDEX_LAPLACIAN);
    glutAddMenuEntry("sharpen", MENU_INDEX_SHARP);
    glutAddMenuEntry("magnified", 9);
    glutAddMenuEntry("pixelization", 10);
    glutAddMenuEntry("fisheye", 11);
    glutAddMenuEntry("gamma", 12);
    glutAddMenuEntry("bloom", 13);
    glutAddMenuEntry("spilled", 14);
    glutAddMenuEntry("kaleidoscope", 15);
    glutAddMenuEntry("better filtering", 16);
    glutAddMenuEntry("spirlt", 17);
    glutAddMenuEntry("swirl", 18);
    glutAddMenuEntry("normal", 999);

	glutSetMenu(menu_main);
	glutAttachMenu(GLUT_RIGHT_BUTTON);

	// Register GLUT callback functions.
	glutDisplayFunc(My_Display);
	glutReshapeFunc(My_Reshape);
	glutMouseFunc(My_Mouse);
	glutKeyboardFunc(My_Keyboard);
	glutSpecialFunc(My_SpecialKeys);
	glutTimerFunc(timer_speed, My_Timer, 0);
    glutMotionFunc(My_Motion);

	// Enter main event loop.
	glutMainLoop();

	return 0;
}
