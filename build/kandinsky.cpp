
//CAUTION: The content of this file is automatically generated by Emacs orgmode
//from the file kandinsky.org that should either be in this, or the parent
//directory. Consequently, any modifications made to this file will likely be
//ephemeral. Please edit kandinsky.org instead.
////////////////////////////////////////////////////////////////////////////////
//
// kandinsky.cpp
//
////////////////////////////////////////////////////////////////////////////////
//
//================================================================================
// This file is part of project kandinsky, a simple demonstration of the OpenGL
// API, that draws 600 colorful translucent triangles, rectangles and ellipses
// on the display. Since the random number seed is initialized to Unix time,
// a different canvas is obtained with each execution.
//
// kandinsky is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// kandinsky is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with union-find.org.  If not, see <http://www.gnu.org/licenses/>.
//
// Copyright (C) 2014, 2015 Robert R. Snapp.
//================================================================================
// This file was automatically generated using an org-babel tangle operation with
// the file kandinsky.org. Thus, the latter file should be edited instead of
// this file.
//================================================================================
//
//
// Standard C++ libraries
#include <iostream>
#include <stdexcept>
#include <cmath>
#include <ctime>
#include <sstream>
#include <cstring>
#include <fstream>
#include <string>
#include <cstdlib>                       // for rand(), srand()
#include <vector>

// Graphics libraries that extend OpenGL
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

// FreeImage headers, for digital image format support.
#include <FreeImage.h>

struct Figure {
    GLuint model;
    glm::vec4 color;
    glm::mat4 transformation;
};

const GLuint N_MODELS = 3;
const GLuint N_Vertices[N_MODELS] = {3, 4, 102};              // number of vertices per model;
GLuint PrimitiveModeToken[N_MODELS] = {GL_TRIANGLES,          // triangle primitive mode
    GL_TRIANGLE_STRIP,     // square primitive mode
    GL_TRIANGLE_FAN};      // circle primitive mode
GLuint VertexArrayObject[N_MODELS];                           // One GPU object per model: initialized in init()

//const int N_FIGURES = 600;
const int N_FIGURES = 3;
std::vector<Figure> Figs;

GLFWwindow* gWindow = NULL;               // pointer to the graphics window goverened by the OS
const glm::vec2 CANVAS(800, 800);         // Our window and viewport dimensions.

const size_t MAX_FILE_MATCH_ATTEMPTS = 1000;

GLuint VSL_color; // vertex shader location for the color uniform variable;
GLuint VSL_transformation; // vertex shader location for the transformation uniform matrix

GLuint loadAndCompileShader(GLenum shaderType, const std::string& path) {
    std::ifstream f;
    f.open(path.c_str(), std::ios::in | std::ios::binary);
    if (!f.is_open()) {
        throw std::runtime_error(std::string("Can't open shader file ") + path);
    }

    // read the shader program from the file into the buffer
    std::stringstream buffer;
    buffer << f.rdbuf();

    GLuint shader = glCreateShader(shaderType);
    if (! shader) {
        throw std::runtime_error(std::string("Can't create shader for file ") + path);
    }

    // tricky conversion from a stringstream buffer to a C (null terminated) string.
    const std::string& bufferAsString = buffer.str();
    const GLchar* shaderCode = bufferAsString.c_str();
    const GLchar* codeArray[] = { shaderCode };
    GLint size = strlen(shaderCode);
    glShaderSource(shader, 1, codeArray, NULL);
    glCompileShader(shader);

    GLint status;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
    if (! status) {
        std::cerr << "Compilation error in shader file " << path << std::endl;
        GLint logLen;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &logLen);
        if (logLen > 0) {
            char *log = new char[logLen];
            GLsizei written;
            glGetShaderInfoLog(shader, logLen, &written, log);
            std::cerr << "Shader log: " << std::endl;
            std::cerr << log << std::endl;
            delete [] log;
        }
        throw std::runtime_error(
                std::string("Can't compile the shader defined in file ") + path);
    }
    return shader;
}

GLuint createVertexFragmentProgram(const std::string& vertex_shader_path, const std::string& fragment_shader_path) {
    GLuint vertexShader   = loadAndCompileShader(GL_VERTEX_SHADER,   vertex_shader_path);
    GLuint fragmentShader = loadAndCompileShader(GL_FRAGMENT_SHADER, fragment_shader_path);

    GLuint program = glCreateProgram();
    if (! program) {
        throw std::runtime_error("Can't create GLSL program.");
    }

    glAttachShader(program, vertexShader);
    glAttachShader(program, fragmentShader);
    glLinkProgram(program);

    GLint status;
    glGetProgramiv(program, GL_LINK_STATUS, &status);
    if (! status) {
        std::cerr << "Linking error in shader program!" << std::endl;
        GLint logLen;
        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &logLen);
        if (logLen > 0) {
            char *log = new char[logLen];
            GLsizei written;
            glGetProgramInfoLog(program, logLen, &written, log);
            std::cerr << "Shader log: " << std::endl;
            std::cerr << log << std::endl;
            delete [] log;
        }
        throw std::runtime_error("Can't link shader program.");
    }
    return program;
}

std::string getImageFileName(const std::string &prefix, const std::string &ext) {
    std::string imageFileName("");  // initial value is returned in search fails.

    // load the current date and time in theTime.
    time_t theTime;
    time(&theTime);

    // convert the date time into the current locale.
    struct tm *theTimeInfo;
    theTimeInfo = localtime(&theTime);

    // represent the date and time in the format "150117184509" for 2015 January 17,
    // 18 hours, 45 minutes, 09 seconds.
    const int TIME_STRING_SIZE = 16;  // must be at least 13.
    char timeString[TIME_STRING_SIZE];
    strftime(timeString, TIME_STRING_SIZE, "%y%m%d%H%M%S", theTimeInfo);

    std::stringstream filename;
    std::ifstream fs;
    int index =0;

    // find a new filename of the form <filenameprefix>_<timeString>_<index>.<ext>, where
    // <index> is incremented as needed.
    do {
        // reset the stringstream filename
        filename.str(std::string());      // clear the buffer content
        filename.clear();                 // reset the state flags

        // propose an available (unused) filename.
        filename << prefix << "_" << timeString << "_" << index++ << ext;

        // Ensure the ifstream fs is closed.
        if (fs.is_open()) fs.close();

        // If file does not exist, it can't be opened in input mode.
        fs.open(filename.str(), std::ios_base::in);
    } while (fs.is_open() && index < MAX_FILE_MATCH_ATTEMPTS);

    // Test to see if a unique name was actually found.
    if (fs.is_open() && index >= MAX_FILE_MATCH_ATTEMPTS) {
        fs.close();  // failure: there are too many files with the same time stamp.
    } else {       // N.B. fs must be closed on this branch, right?
        imageFileName = filename.str();  // success!
    }
    return imageFileName;
}

void exportPNGImage(GLFWwindow* window, const std::string &prefix) {
    std::string imagefilename = getImageFileName(prefix, std::string(".png"));
    if (imagefilename.length() == 0) {
        throw std::runtime_error("Can't find an available png file name.");
    }

    // Recover window dimensions from glfw
    int width;
    int height;
    glfwGetFramebufferSize(window, &width, &height);

    // allocate a buffer to store all of the pixels in the frame buffer.
    GLubyte* pixels = new GLubyte [width * height * sizeof(GLubyte)* 3];
    if (! pixels) {
        throw std::runtime_error(std::string("Can't allocate pixel array for image of size ")
                + std::to_string(width) + std::string(" by ") + std::to_string(height)
                + std::string(" pixels."));
    }

    glPixelStorei(GL_PACK_ALIGNMENT, 1); // Align data by bytes so the framebuffer data will fit.
    glReadBuffer(GL_FRONT);              // The front color buffer is the visible one.
    glReadPixels(0, 0, width, height, GL_BGR, GL_UNSIGNED_BYTE, pixels);

    // The following variables are introduced to clarify the call signature of
    // FreeImage_ConvertFromRawBits, which will convert the GLubyte pixel buffer
    // into a FreeImage buffer.
    const unsigned int red_mask             = 0xFF0000;
    const unsigned int green_mask           = 0x00FF00;
    const unsigned int blue_mask            = 0x0000FF;
    const unsigned int bits_per_pixel       = 24;
    const bool top_left_pixel_appears_first = false;

    unsigned int bytes_per_row = 3 * width;

    FIBITMAP* image = FreeImage_ConvertFromRawBits(pixels, width, height, bytes_per_row,
            bits_per_pixel, red_mask, green_mask,
            blue_mask, top_left_pixel_appears_first);

    // Save the image as a png file.
    FreeImage_Save(FIF_PNG, image, imagefilename.c_str(), 0);

    // Free allocated memory
    FreeImage_Unload(image);
    delete [] pixels;
    return;
}

glm::mat4 getTransformation(GLfloat sx, GLfloat sy, GLfloat theta, GLfloat tx, GLfloat ty) {
    glm::mat4 identity_mtx = glm::mat4(1.0f);
    glm::mat4 translate_mtx = glm::translate(identity_mtx, glm::vec3(tx, ty, 0.0f));
    glm::mat4 translate_rotate_mtx = glm::rotate(translate_mtx, theta, glm::vec3(0.0f, 0.0f, 1.0f));
    glm::mat4 translate_rotate_scale_mtx = glm::scale(translate_rotate_mtx, glm::vec3(sx, sy, 1.0f));
    return translate_rotate_scale_mtx;
}

glm::mat4 getRandomTransformation() {
    GLfloat sx    = 0.10*static_cast<GLfloat>(rand())/RAND_MAX + 0.05f;
    GLfloat sy    = 0.10*static_cast<GLfloat>(rand())/RAND_MAX + 0.05f;
    GLfloat theta = 2.0*M_PI*static_cast<GLfloat>(rand())/RAND_MAX;
    GLfloat tx    = 2.0*static_cast<GLfloat>(rand())/RAND_MAX - 1.0;
    GLfloat ty    = 2.0*static_cast<GLfloat>(rand())/RAND_MAX - 1.0;

    return getTransformation(sx, sy, theta, tx, ty);
}

glm::vec4 getColor(int red, int green, int blue, GLfloat alpha) {
    return glm::vec4(
            static_cast<GLfloat>(red)/255,
            static_cast<GLfloat>(green)/255,
            static_cast<GLfloat>(blue)/255,
            alpha);
}

glm::vec4 getRandomColor() {
    GLfloat red   = static_cast<GLfloat>(rand())/RAND_MAX;
    GLfloat green = static_cast<GLfloat>(rand())/RAND_MAX;
    GLfloat blue  = static_cast<GLfloat>(rand())/RAND_MAX;
    GLfloat alpha = 0.75*static_cast<GLfloat>(rand())/RAND_MAX + 0.25;  // not too transparent.
    return glm::vec4(red, green, blue, alpha);
}


void init(void) {
    GLfloat triangle_positions[][2] = {
        { 0.0f, 0.0f },   // vertices ordered for a TRIANGLE
        { 1.0f, 0.0f },
        { 0.0f, 1.0f },
    };

    GLfloat square_positions[][2] = {
        { -0.5f, -0.5f },  // vertices ordered for a TRIANGLE_STRiP
        { -0.5f,  0.5f },
        {  0.5f, -0.5f },
        {  0.5f,  0.5f },
    };

    GLfloat circle_positions[102][2];
    circle_positions[0][0] = 0.0f;
    circle_positions[0][1] = 0.0f;
    double delta = 2.0*M_PI/100;
    for (int i = 0; i < 101; i++) {
        circle_positions[i+1][0] = cos(i*delta);
        circle_positions[i+1][1] = sin(i*delta);
    }

    srand(time(NULL)); // initialize the random number generator rand(), using time as seed.

    int models[N_FIGURES];
    models[0] = 2;
    models[1] = 2;
    models[2] = 2;

    glm::vec4 colors[N_FIGURES];
    colors[0] = getColor(255, 0, 0, 1);
    colors[1] = getColor(0, 255, 0, 1);
    colors[2] = getColor(0, 0, 255, 1);

    glm::mat4 transformations[N_FIGURES];
    transformations[0] = getTransformation(0.5, 0.5, 0, 0, 0);
    transformations[1] = getTransformation(0.4, 0.4, 0, 0, 0);
    transformations[2] = getTransformation(0.3, 0.3, 0, 0, 0);


    for (int i = 0; i < sizeof(models) / sizeof(int); i++) {
        Figure fig;
        fig.model = models[i];
        fig.color = colors[i];
        fig.transformation = transformations[i];
        Figs.push_back(fig);
    }

    GLuint vboHandles[N_MODELS];
    glGenBuffers(N_MODELS, vboHandles);

    GLuint triangle_position_buffer = vboHandles[0];
    GLuint square_position_buffer   = vboHandles[1];
    GLuint circle_position_buffer   = vboHandles[2];

    glGenVertexArrays(N_MODELS, VertexArrayObject);

    glBindVertexArray(VertexArrayObject[0]);
    glEnableVertexAttribArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, triangle_position_buffer);
    glBufferData(GL_ARRAY_BUFFER, sizeof(triangle_positions), triangle_positions, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, NULL);

    glBindVertexArray(VertexArrayObject[1]);
    glEnableVertexAttribArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, square_position_buffer);
    glBufferData(GL_ARRAY_BUFFER, sizeof(square_positions), square_positions, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, NULL);

    glBindVertexArray(VertexArrayObject[2]);
    glEnableVertexAttribArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, circle_position_buffer);
    glBufferData(GL_ARRAY_BUFFER, sizeof(circle_positions), circle_positions, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, NULL);

    GLuint shader_program = createVertexFragmentProgram(std::string("kandinsky.vert"),
            std::string("kandinsky.frag"));
    glUseProgram(shader_program);
    VSL_color          = glGetUniformLocation(shader_program, "color");
    VSL_transformation = glGetUniformLocation(shader_program, "transformation");
}
//------------------------------------------------------------------------------
//
// display() should be called whenever the canvas needs to be refreshed. Here it
// redraws the content of the Figs vector,

void display(void) {
    glClearColor(0.5,0.5,0.5,1); // gray
    glClear(GL_COLOR_BUFFER_BIT);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    for (int i = 0; i < N_FIGURES; i++) {
        glUniform4fv(VSL_color, 1, &Figs[i].color[0]);
        glUniformMatrix4fv(VSL_transformation, 1, GL_FALSE, &Figs[i].transformation[0][0]);
        glBindVertexArray(VertexArrayObject[Figs[i].model]);
        glDrawArrays(PrimitiveModeToken[Figs[i].model], 0, N_Vertices[Figs[i].model]);
    }

    glfwSwapBuffers(gWindow);
}

void keyboard(GLFWwindow* window, int keyCode, int scanCode, int action, int modifiers) {
    switch (keyCode) {
        case GLFW_KEY_P:
            if (action == GLFW_PRESS && modifiers == 0x0000) {
                std::cout << "Pressed a lower-case p, scanCode = " << scanCode << std::endl;
                exportPNGImage(window, std::string("kandinsky"));
            }
            break;
        case GLFW_KEY_Q:
            if (action == GLFW_PRESS && modifiers == 0x0000) {
                std::cout << "Pressed a lower-case q, scanCode = " << scanCode << std::endl;
                glfwSetWindowShouldClose(window, GL_TRUE);
            }
            break;
    }
    return;
}

//------------------------------------------------------------------------------
//
// error_callback is used by GLFW.

void GLFW_error_callback(int errorCode, const char* msg) {
    throw std::runtime_error(msg);
}

//------------------------------------------------------------------------------
//
// free image error callback
void fi_error_callback(FREE_IMAGE_FORMAT fif, const char* msg) {
    if (fif != FIF_UNKNOWN) {
        std:: cerr << FreeImage_GetFormatFromFIF(fif) << " format." << std:: endl;
    }
    throw std::runtime_error(msg);
}

int main(int argc, char** argv) {
    // initialize GLFW
    glfwSetErrorCallback(GLFW_error_callback);
    if (!glfwInit())
        throw std::runtime_error("glfwInit failed!");

    // Create the main window
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
    glfwWindowHint(GLFW_RESIZABLE, GL_FALSE);

    int glfw_major;
    int glfw_minor;
    int glfw_rev;
    glfwGetVersion(&glfw_major, &glfw_minor, &glfw_rev);
    std::cout << "GLFW version: " << glfw_major << "." << glfw_minor << "." << glfw_rev << std::endl;

    gWindow = glfwCreateWindow((int) CANVAS.x, (int) CANVAS.y, argv[0], NULL, NULL);
    if (! gWindow)
        throw std::runtime_error("Can't create a glfw window!");

    glfwMakeContextCurrent(gWindow);

    glfwSetKeyCallback(gWindow, keyboard);
    glewExperimental = GL_TRUE;  // Prevents a segmentation fault on Mac OSX
    if (glewInit() != GLEW_OK)
        throw std::runtime_error("Can't initialize glewInit!");

    std::cout << "OpenGL version: "  << glGetString(GL_VERSION) << std::endl;
    std::cout << "GLSL version: "    << glGetString(GL_SHADING_LANGUAGE_VERSION) << std::endl;
    std::cout << "Vendor: "          << glGetString(GL_VENDOR) << std::endl;
    std::cout << "Graphics engine: " << glGetString(GL_RENDERER) << std::endl;

    if (! GLEW_VERSION_3_2)
        throw std::runtime_error("OpenGL 3.2 is not supported!");

    // Initializing FreeImage:
    FreeImage_Initialise(TRUE);                   // only load local plugins.
    FreeImage_SetOutputMessage(fi_error_callback);

    // Let's see the version of FreeImage:
    std::cout << "FreeImage version = " << FreeImage_GetVersion() << std::endl;

    init(); // Initialize the model.

    int update_count = 0;
    while(! glfwWindowShouldClose(gWindow)) {
        update_count++;
        display();
        glfwWaitEvents();     // or, replace with glfwPollEvents();
    }

    FreeImage_DeInitialise();
    glfwTerminate();
    std::cout << argv[0] << " gracefully exits after " << update_count << " window updates." << std::endl;
    return 0; // Can be safely omitted, but the application will still return 0.
}

// end of kandinsky.cpp
