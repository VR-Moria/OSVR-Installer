/** @file
    @brief Example program that uses the OSVR direct-to-display interface
           and OpenGL to render a scene with textured characters in it using
           the Freetype library.

    @date 2020

    @author
    Russ Taylor <russ@reliasolve.com>
*/

/** Based on
    @brief Example program that uses the OSVR direct-to-display interface
           and OpenGL to render a scene with low latency.

    @date 2015

    @author
    Russ Taylor <russ@sensics.com>
    <http://sensics.com/osvr>
*/

// Copyright 2015 Sensics, Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// Internal Includes
#include <osvr/RenderKit/RenderManager.h>
#include <osvr/ClientKit/Context.h>

// Library/third-party includes
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#include <GL/glew.h>

#include <freetype2/ft2build.h>
#include FT_FREETYPE_H

// Standard includes
#include <iostream>
#include <string>
#include <stdlib.h> // For exit()

// This must come after we include <GL/gl.h> so its pointer types are defined.
#include <osvr/RenderKit/GraphicsLibraryOpenGL.h>
#include <osvr/RenderKit/RenderKitGraphicsTransforms.h>

// normally you'd load the shaders from a file, but in this case, let's
// just keep things simple and load from memory.
static const GLchar* vertexShader =
    "#version 330 core\n"
    "layout(location = 0) in vec3 position;\n"
    "layout(location = 1) in vec3 vertexColor;\n"
    "layout(location = 2) in vec2 vertexTextureCoord;\n"
    "out vec3 fragmentColor;\n"
    "out vec2 textureCoord;\n"
    "uniform mat4 modelView;\n"
    "uniform mat4 projection;\n"
    "void main()\n"
    "{\n"
    "   gl_Position = projection * modelView * vec4(position,1);\n"
    "   fragmentColor = vertexColor;\n"
    "   textureCoord = vertexTextureCoord;\n"
    "}\n";

static const GLchar* fragmentShader =
    "#version 330 core\n"
    "in vec3 fragmentColor;\n"
    "in vec2 textureCoord;\n"
    "layout(location = 0) out vec3 color;\n"
    "uniform sampler2D tex;\n"
    "void main()\n"
    "{\n"
    "   color = fragmentColor;\n"
    "   //color = fragmentColor * texture(tex, textureCoord).rgb;\n"
    "}\n";

class SampleShader {
  public:
    SampleShader() {}

    ~SampleShader() {
        if (initialized) {
            glDeleteProgram(programId);
        }
    }

    void init() {
        if (!initialized) {
            GLuint vertexShaderId = glCreateShader(GL_VERTEX_SHADER);
            GLuint fragmentShaderId = glCreateShader(GL_FRAGMENT_SHADER);

            // vertex shader
            glShaderSource(vertexShaderId, 1, &vertexShader, NULL);
            glCompileShader(vertexShaderId);
            checkShaderError(vertexShaderId,
                             "Vertex shader compilation failed.");

            // fragment shader
            glShaderSource(fragmentShaderId, 1, &fragmentShader, NULL);
            glCompileShader(fragmentShaderId);
            checkShaderError(fragmentShaderId,
                             "Fragment shader compilation failed.");

            // linking program
            programId = glCreateProgram();
            glAttachShader(programId, vertexShaderId);
            glAttachShader(programId, fragmentShaderId);
            glLinkProgram(programId);
            checkProgramError(programId, "Shader program link failed.");

            // once linked into a program, we no longer need the shaders.
            glDeleteShader(vertexShaderId);
            glDeleteShader(fragmentShaderId);

            projectionUniformId = glGetUniformLocation(programId, "projection");
            modelViewUniformId = glGetUniformLocation(programId, "modelView");
            initialized = true;
        }
    }

    void useProgram(const GLdouble projection[], const GLdouble modelView[]) {
        init();
        glUseProgram(programId);
        GLfloat projectionf[16];
        GLfloat modelViewf[16];
        convertMatrix(projection, projectionf);
        convertMatrix(modelView, modelViewf);
        glUniformMatrix4fv(projectionUniformId, 1, GL_FALSE, projectionf);
        glUniformMatrix4fv(modelViewUniformId, 1, GL_FALSE, modelViewf);
    }

  private:
    SampleShader(const SampleShader&) = delete;
    SampleShader& operator=(const SampleShader&) = delete;
    bool initialized = false;
    GLuint programId = 0;
    GLuint projectionUniformId = 0;
    GLuint modelViewUniformId = 0;

    void checkShaderError(GLuint shaderId, const std::string& exceptionMsg) {
        GLint result = GL_FALSE;
        int infoLength = 0;
        glGetShaderiv(shaderId, GL_COMPILE_STATUS, &result);
        glGetShaderiv(shaderId, GL_INFO_LOG_LENGTH, &infoLength);
        if (result == GL_FALSE) {
            std::vector<GLchar> errorMessage(infoLength + 1);
            glGetProgramInfoLog(programId, infoLength, NULL, &errorMessage[0]);
            std::cerr << &errorMessage[0] << std::endl;
            throw std::runtime_error(exceptionMsg);
        }
    }

    void checkProgramError(GLuint programId, const std::string& exceptionMsg) {
        GLint result = GL_FALSE;
        int infoLength = 0;
        glGetProgramiv(programId, GL_LINK_STATUS, &result);
        glGetProgramiv(programId, GL_INFO_LOG_LENGTH, &infoLength);
        if (result == GL_FALSE) {
            std::vector<GLchar> errorMessage(infoLength + 1);
            glGetProgramInfoLog(programId, infoLength, NULL, &errorMessage[0]);
            std::cerr << &errorMessage[0] << std::endl;
            throw std::runtime_error(exceptionMsg);
        }
    }

    void convertMatrix(const GLdouble source[], GLfloat dest_out[]) {
        if (nullptr == source || nullptr == dest_out) {
            throw new std::logic_error("source and dest_out must be non-null.");
        }
        for (int i = 0; i < 16; i++) {
            dest_out[i] = (GLfloat)source[i];
        }
    }
};
static SampleShader sampleShader;

// Things needed for Freetype font display
FT_Library g_ft = nullptr;
FT_Face g_face = nullptr;
#ifdef _WIN32
  std::vector<const char*> FONTS = {"C:/Windows/Fonts/arial.ttf"};
#elif defined(__APPLE__)
  std::vector<const char*> FONTS = {"/System/Library/Fonts/NewYork.ttf"};
#else
  std::vector<const char*> FONTS = {
    "/usr/share/fonts/truetype/ubuntu-font-family/Ubuntu-R.ttf",
    "/usr/share/fonts/truetype/ubuntu/Ubuntu-R.ttf"
  };
#endif
const int FONT_SIZE = 48;
GLuint g_font_tex = 0;
GLuint g_fontShader = 0;
GLuint g_fontVertexBuffer = 0;

class FontVertex {
public:
  GLfloat pos[3];
  GLfloat col[4];
  GLfloat tex[2];
};

static void addFontQuad(std::vector<FontVertex> &vertexBufferData,
  GLfloat left, GLfloat right, GLfloat top, GLfloat bottom, GLfloat depth,
  GLfloat R, GLfloat G, GLfloat B, GLfloat alpha)
{
  FontVertex v;
  v.col[0] = R; v.col[1] = G; v.col[2] = B; v.col[3] = alpha;

  // Invert the Y texture coordinate so that we draw the textures
  // right-side up.
  // Switch the order so we have clockwise front-facing.
  v.pos[0] = left; v.pos[1] = bottom; v.pos[2] = depth;
  v.tex[0] = 0; v.tex[1] = 1;
  vertexBufferData.emplace_back(v);
  v.pos[0] = right; v.pos[1] = top; v.pos[2] = depth;
  v.tex[0] = 1; v.tex[1] = 0;
  vertexBufferData.emplace_back(v);
  v.pos[0] = right; v.pos[1] = bottom; v.pos[2] = depth;
  v.tex[0] = 1; v.tex[1] = 1;
  vertexBufferData.emplace_back(v);

  v.pos[0] = left; v.pos[1] = bottom; v.pos[2] = depth;
  v.tex[0] = 0; v.tex[1] = 1;
  vertexBufferData.emplace_back(v);
  v.pos[0] = left; v.pos[1] = top; v.pos[2] = depth;
  v.tex[0] = 0; v.tex[1] = 0;
  vertexBufferData.emplace_back(v);
  v.pos[0] = right; v.pos[1] = top; v.pos[2] = depth;
  v.tex[0] = 1; v.tex[1] = 0;
  vertexBufferData.emplace_back(v);
}

void render_text(const GLdouble projection[], const GLdouble modelView[],
    const char *text, float x, float y, float sx, float sy)
{
  if (!g_face) { return; }
  FT_GlyphSlot g = g_face->glyph;
  GLenum err;

  // Use the font shader to render this.  It may activate a different texture unit, so we
  // need to make sure we active the first one once we are using the program.
  //sampleShader.useProgram(projection, modelView);

  err = glGetError();
  if (err != GL_NO_ERROR) {
    std::cerr << "render_text(): Error after use program: "
      << err << std::endl;
  }

  glActiveTexture(GL_TEXTURE0);

  err = glGetError();
  if (err != GL_NO_ERROR) {
    std::cerr << "render_text(): Error after texture set: "
      << err << std::endl;
  }

  std::vector<FontVertex> vertexBufferData;

  // Enable blending using alpha.
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_COLOR, GL_ONE_MINUS_SRC_ALPHA);

  err = glGetError();
  if (err != GL_NO_ERROR) {
    std::cerr << "render_text(): Error after blend enable: "
      << err << std::endl;
  }

  // Blend in a black rectangle that partially covers the region behind it, and which the
  // text will be drawn above.  Flip it upside down so that its vertices will show up as
  // front facing when it is re-flipped in the addFontQuad() method.
  glBindTexture(GL_TEXTURE_2D, 0);
  FT_Load_Char(g_face, '0', FT_LOAD_RENDER);
  float w = g->bitmap.width * sx;
  float h = g->bitmap.rows * sy;
  size_t chars = (strlen(text) + 1);
  vertexBufferData.clear();
  addFontQuad(vertexBufferData, x, x + chars*w, y+h, y, 0.6f, 0,0,0,0.5f);
  glBindBuffer(GL_ARRAY_BUFFER, g_fontVertexBuffer);
  glBufferData(GL_ARRAY_BUFFER,
    sizeof(vertexBufferData[0]) * vertexBufferData.size(),
    &vertexBufferData[0], GL_STATIC_DRAW);

  err = glGetError();
  if (err != GL_NO_ERROR) {
    std::cerr << "render_text(): Error after buffering data: "
      << err << std::endl;
  }

  // Configure the vertex-buffer objects.
  {
    size_t const stride = sizeof(vertexBufferData[0]);
    // VBO
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride,
      (GLvoid*)(offsetof(FontVertex, pos)));

    // color
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, stride,
      (GLvoid*)(offsetof(FontVertex, col)));

    // texture
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, stride,
      (GLvoid*)(offsetof(FontVertex, tex)));
  }

  // Draw the quad.
  {
    GLsizei numElements = static_cast<GLsizei>(vertexBufferData.size());
    glDrawArrays(GL_TRIANGLES, 0, numElements);
  }
  glBindBuffer(GL_ARRAY_BUFFER, 0);
  err = glGetError();
  if (err != GL_NO_ERROR) {
    std::cerr << "render_text(): Error after mask: "
      << err << std::endl;
  }

  // Generate the font texture if we don't yet have it.  In any case, bind it as
  // the active texture.
  if (!g_font_tex) {
    // Note that we don't really have a good place to destroy this texture
    // because the rendering window is destroyed by each subclass in its own
    // main() thread instance and it would have to be destroyed just before that
    // in each.  When the window is destroyed, hopefully all of its texture
    // resources are also cleaned up by the driver.
    glGenTextures(1, &g_font_tex);
  }
  glBindTexture(GL_TEXTURE_2D, g_font_tex);
  err = glGetError();
  if (err != GL_NO_ERROR) {
    std::cerr << "render_text(): Error binding texture: "
      << err << std::endl;
  }

  // Blend the characters in, so we see them written above the background.
  // We use color for the alpha channel so it appears wherever the character appears.
  // Go through each character and render it until we get to the NULL terminator.
  for (const char *p = text; *p; p++) {
    if (FT_Load_Char(g_face, *p, FT_LOAD_RENDER))
      continue;

    // Set the parameters we need to render the text properly.
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, g->bitmap.width);
    err = glGetError();
    if (err != GL_NO_ERROR) {
      std::cerr << "render_text(): Error setting texture params: "
        << err << std::endl;
    }
    glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, g->bitmap.width, g->bitmap.rows,
      0, GL_LUMINANCE, GL_UNSIGNED_BYTE, g->bitmap.buffer);
    err = glGetError();
    if (err != GL_NO_ERROR) {
      std::cerr << "render_text(): Error writing texture: "
        << err << std::endl;
    }
    float x2 = x + g->bitmap_left * sx;
    float y2 = y + g->bitmap_top * sy;
    float w = g->bitmap.width * sx;
    float h = g->bitmap.rows * sy;

    // Blend in the text, fully opaque (inverse alpha) and fully white.
    vertexBufferData.clear();
    addFontQuad(vertexBufferData, x2, x2 + w, y2, y2 - h, 0.7f, 1,1,1,0);
    glBindBuffer(GL_ARRAY_BUFFER, g_fontVertexBuffer);
    glBufferData(GL_ARRAY_BUFFER,
      sizeof(vertexBufferData[0]) * vertexBufferData.size(),
      &vertexBufferData[0], GL_STATIC_DRAW);
    err = glGetError();
    if (err != GL_NO_ERROR) {
      std::cerr << "render_text(): Error buffering data: "
        << err << std::endl;
    }

    // Configure the vertex-buffer objects.
    {
      size_t const stride = sizeof(vertexBufferData[0]);
      // VBO
      glEnableVertexAttribArray(0);
      glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride,
        (GLvoid*)(offsetof(FontVertex, pos)));

      // color
      glEnableVertexAttribArray(1);
      glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, stride,
        (GLvoid*)(offsetof(FontVertex, col)));

      // texture
      glEnableVertexAttribArray(2);
      glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, stride,
        (GLvoid*)(offsetof(FontVertex, tex)));
    }

    // Draw the quad.
    {
      GLsizei numElements = static_cast<GLsizei>(vertexBufferData.size());
      glDrawArrays(GL_TRIANGLES, 0, numElements);
    }
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    x += (g->advance.x / 64) * sx;
    y += (g->advance.y / 64) * sy;
  }

  // Set things back to the defaults
  glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
  glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);

  glBindTexture(GL_TEXTURE_2D, 0);
  glDisable(GL_BLEND);
}

class Cube {
  public:
    Cube(GLfloat scale) {
        colorBufferData = {1.0, 0.0, 0.0, // +X
                           1.0, 0.0, 0.0, 1.0, 0.0, 0.0,

                           1.0, 0.0, 0.0, 1.0, 0.0, 0.0, 1.0, 0.0, 0.0,

                           1.0, 0.0, 1.0, // -X
                           1.0, 0.0, 1.0, 1.0, 0.0, 1.0,

                           1.0, 0.0, 1.0, 1.0, 0.0, 1.0, 1.0, 0.0, 1.0,

                           0.0, 1.0, 0.0, // +Y
                           0.0, 1.0, 0.0, 0.0, 1.0, 0.0,

                           0.0, 1.0, 0.0, 0.0, 1.0, 0.0, 0.0, 1.0, 0.0,

                           1.0, 1.0, 0.0, // -Y
                           1.0, 1.0, 0.0, 1.0, 1.0, 0.0,

                           1.0, 1.0, 0.0, 1.0, 1.0, 0.0, 1.0, 1.0, 0.0,

                           0.0, 0.0, 1.0, // +Z
                           0.0, 0.0, 1.0, 0.0, 0.0, 1.0,

                           0.0, 0.0, 1.0, 0.0, 0.0, 1.0, 0.0, 0.0, 1.0,

                           0.0, 1.0, 1.0, // -Z
                           0.0, 1.0, 1.0, 0.0, 1.0, 1.0,

                           0.0, 1.0, 1.0, 0.0, 1.0, 1.0, 0.0, 1.0, 1.0};

        vertexBufferData = {scale,  scale,  scale, // +X
                            scale,  -scale, -scale, scale,  scale,  -scale,

                            scale,  -scale, -scale, scale,  scale,  scale,
                            scale,  -scale, scale,

                            -scale, -scale, -scale, // -X
                            -scale, -scale, scale,  -scale, scale,  scale,

                            -scale, -scale, -scale, -scale, scale,  scale,
                            -scale, scale,  -scale,

                            scale,  scale,  scale, // +Y
                            scale,  scale,  -scale, -scale, scale,  -scale,

                            scale,  scale,  scale,  -scale, scale,  -scale,
                            -scale, scale,  scale,

                            scale,  -scale, scale, // -Y
                            -scale, -scale, -scale, scale,  -scale, -scale,

                            scale,  -scale, scale,  -scale, -scale, scale,
                            -scale, -scale, -scale,

                            -scale, scale,  scale, // +Z
                            -scale, -scale, scale,  scale,  -scale, scale,

                            scale,  scale,  scale,  -scale, scale,  scale,
                            scale,  -scale, scale,

                            scale,  scale,  -scale, // -Z
                            -scale, -scale, -scale, -scale, scale,  -scale,

                            scale,  scale,  -scale, scale,  -scale, -scale,
                            -scale, -scale, -scale};
    }

    ~Cube() {
        if (initialized) {
            glDeleteBuffers(1, &vertexBuffer);
            glDeleteVertexArrays(1, &vertexArrayId);
        }
    }

    void init() {
        if (!initialized) {
            // Vertex buffer
            glGenBuffers(1, &vertexBuffer);
            glBindBuffer(GL_ARRAY_BUFFER, vertexBuffer);
            glBufferData(GL_ARRAY_BUFFER,
                         sizeof(vertexBufferData[0]) * vertexBufferData.size(),
                         &vertexBufferData[0], GL_STATIC_DRAW);
            glBindBuffer(GL_ARRAY_BUFFER, 0);

            // Color buffer
            glGenBuffers(1, &colorBuffer);
            glBindBuffer(GL_ARRAY_BUFFER, colorBuffer);
            glBufferData(GL_ARRAY_BUFFER,
                         sizeof(colorBufferData[0]) * colorBufferData.size(),
                         &colorBufferData[0], GL_STATIC_DRAW);
            glBindBuffer(GL_ARRAY_BUFFER, 0);

            // Vertex array object
            glGenVertexArrays(1, &vertexArrayId);
            glBindVertexArray(vertexArrayId);
            {
                // color
                glBindBuffer(GL_ARRAY_BUFFER, colorBuffer);
                glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 0, (GLvoid*)0);

                // VBO
                glBindBuffer(GL_ARRAY_BUFFER, vertexBuffer);
                glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, (GLvoid*)0);

                glEnableVertexAttribArray(0);
                glEnableVertexAttribArray(1);
            }
            glBindVertexArray(0);
            initialized = true;
        }
    }

    void draw(const GLdouble projection[], const GLdouble modelView[]) {
        init();

        sampleShader.useProgram(projection, modelView);

        glBindVertexArray(vertexArrayId);
        {
            glDrawArrays(GL_TRIANGLES, 0,
                         static_cast<GLsizei>(vertexBufferData.size()));
        }
        glBindVertexArray(0);
    }

  private:
    Cube(const Cube&) = delete;
    Cube& operator=(const Cube&) = delete;
    bool initialized = false;
    GLuint colorBuffer = 0;
    GLuint vertexBuffer = 0;
    GLuint vertexArrayId = 0;
    std::vector<GLfloat> colorBufferData;
    std::vector<GLfloat> vertexBufferData;
};

static Cube roomCube(5.0f);
static Cube handsCube(0.05f);

// Set to true when it is time for the application to quit.
// Handlers below that set it to true when the user causes
// any of a variety of events so that we shut down the system
// cleanly.  This only works on Windows.
static bool quit = false;

#ifdef _WIN32
// Note: On Windows, this runs in a different thread from
// the main application.
static BOOL CtrlHandler(DWORD fdwCtrlType)
{
    switch (fdwCtrlType) {
    // Handle the CTRL-C signal.
    case CTRL_C_EVENT:
    // CTRL-CLOSE: confirm that the user wants to exit.
    case CTRL_CLOSE_EVENT:
    case CTRL_BREAK_EVENT:
    case CTRL_LOGOFF_EVENT:
    case CTRL_SHUTDOWN_EVENT:
        quit = true;
        return TRUE;
    default:
        return FALSE;
    }
}
#endif

// This callback sets a boolean value whose pointer is passed in to
// the state of the button that was pressed.  This lets the callback
// be used to handle any button press that just needs to update state.
void myButtonCallback(void* userdata, const OSVR_TimeValue* /*timestamp*/,
                      const OSVR_ButtonReport* report)
{
    bool* result = static_cast<bool*>(userdata);
    *result = (report->state != 0);
}

bool SetupRendering(osvr::renderkit::GraphicsLibrary library)
{
    // Make sure our pointers are filled in correctly.
    if (library.OpenGL == nullptr) {
        std::cerr << "SetupRendering: No OpenGL GraphicsLibrary, this should "
                     "not happen"
                  << std::endl;
        return false;
    }

    osvr::renderkit::GraphicsLibraryOpenGL* glLibrary = library.OpenGL;

    // Turn on depth testing, so we get correct ordering.
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    return true;
}

// Callback to set up a given display, which may have one or more eyes in it
void SetupDisplay(
    void* userData //< Passed into SetDisplayCallback
    , osvr::renderkit::GraphicsLibrary library //< Graphics library context to use
    , osvr::renderkit::RenderBuffer buffers //< Buffers to use
    )
{
    // Make sure our pointers are filled in correctly.  The config file selects
    // the graphics library to use, and may not match our needs.
    if (library.OpenGL == nullptr) {
        std::cerr
            << "SetupDisplay: No OpenGL GraphicsLibrary, this should not happen"
            << std::endl;
        return;
    }
    if (buffers.OpenGL == nullptr) {
        std::cerr
            << "SetupDisplay: No OpenGL RenderBuffer, this should not happen"
            << std::endl;
        return;
    }

    osvr::renderkit::GraphicsLibraryOpenGL* glLibrary = library.OpenGL;

    // Clear the screen to black and clear depth
    glClearColor(0, 0, 0, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

// Callback to set up for rendering into a given eye (viewpoint and projection).
void SetupEye(
    void* userData //< Passed into SetViewProjectionCallback
    , osvr::renderkit::GraphicsLibrary library //< Graphics library context to use
    , osvr::renderkit::RenderBuffer buffers //< Buffers to use
    , osvr::renderkit::OSVR_ViewportDescription
        viewport //< Viewport set by RenderManager
    , osvr::renderkit::OSVR_ProjectionMatrix
        projection //< Projection matrix set by RenderManager
    , size_t whichEye //< Which eye are we setting up for?
    )
{
    // Make sure our pointers are filled in correctly.  The config file selects
    // the graphics library to use, and may not match our needs.
    if (library.OpenGL == nullptr) {
        std::cerr
            << "SetupEye: No OpenGL GraphicsLibrary, this should not happen"
            << std::endl;
        return;
    }
    if (buffers.OpenGL == nullptr) {
        std::cerr << "SetupEye: No OpenGL RenderBuffer, this should not happen"
                  << std::endl;
        return;
    }

    // Set the viewport
    glViewport(static_cast<GLint>(viewport.left),
      static_cast<GLint>(viewport.lower),
      static_cast<GLint>(viewport.width),
      static_cast<GLint>(viewport.height));
}

// Callbacks to draw things in world space.
void DrawWorld(
    void* userData //< Passed into AddRenderCallback
    , osvr::renderkit::GraphicsLibrary library //< Graphics library context to use
    , osvr::renderkit::RenderBuffer buffers //< Buffers to use
    , osvr::renderkit::OSVR_ViewportDescription
        viewport //< Viewport we're rendering into
    , OSVR_PoseState pose //< OSVR ModelView matrix set by RenderManager
    , osvr::renderkit::OSVR_ProjectionMatrix
        projection //< Projection matrix set by RenderManager
    , OSVR_TimeValue deadline //< When the frame should be sent to the screen
    )
{
    // Make sure our pointers are filled in correctly.  The config file selects
    // the graphics library to use, and may not match our needs.
    if (library.OpenGL == nullptr) {
        std::cerr
            << "DrawWorld: No OpenGL GraphicsLibrary, this should not happen"
            << std::endl;
        return;
    }
    if (buffers.OpenGL == nullptr) {
        std::cerr << "DrawWorld: No OpenGL RenderBuffer, this should not happen"
                  << std::endl;
        return;
    }

    osvr::renderkit::GraphicsLibraryOpenGL* glLibrary = library.OpenGL;

    GLdouble projectionGL[16];
    osvr::renderkit::OSVR_Projection_to_OpenGL(projectionGL, projection);

    GLdouble viewGL[16];
    osvr::renderkit::OSVR_PoseState_to_OpenGL(viewGL, pose);

    /// Draw a cube with a 5-meter radius as the room we are floating in.
    roomCube.draw(projectionGL, viewGL);

    render_text(projectionGL, viewGL, "Hello, World", 0,0, 1,1);
}

// This is used to draw both hands, but a different callback could be
// provided for each hand if desired.
void DrawHand(
    void* userData //< Passed into AddRenderCallback
    , osvr::renderkit::GraphicsLibrary library //< Graphics library context to use
    , osvr::renderkit::RenderBuffer buffers //< Buffers to use
    , osvr::renderkit::OSVR_ViewportDescription
        viewport //< Viewport we're rendering into
    , OSVR_PoseState pose //< OSVR ModelView matrix set by RenderManager
    , osvr::renderkit::OSVR_ProjectionMatrix
        projection //< Projection matrix set by RenderManager
    , OSVR_TimeValue deadline //< When the frame should be sent to the screen
    ) {
    // Make sure our pointers are filled in correctly.  The config file selects
    // the graphics library to use, and may not match our needs.
    if (library.OpenGL == nullptr)
{
        std::cerr
            << "DrawHand: No OpenGL GraphicsLibrary, this should not happen"
            << std::endl;
        return;
    }
    if (buffers.OpenGL == nullptr) {
        std::cerr << "DrawHand: No OpenGL RenderBuffer, this should not happen"
                  << std::endl;
        return;
    }

    osvr::renderkit::GraphicsLibraryOpenGL* glLibrary = library.OpenGL;

    GLdouble projectionGL[16];
    osvr::renderkit::OSVR_Projection_to_OpenGL(projectionGL, projection);

    GLdouble viewGL[16];
    osvr::renderkit::OSVR_PoseState_to_OpenGL(viewGL, pose);
    handsCube.draw(projectionGL, viewGL);
}

int main(int argc, char* argv[])
{
    // Get an OSVR client context to use to access the devices
    // that we need.
    osvr::clientkit::ClientContext context(
        "com.osvr.renderManager.openGLExample");

    // Construct button devices and connect them to a callback
    // that will set the "quit" variable to true when it is
    // pressed.  Use button "1" on the left-hand or
    // right-hand controller.
    osvr::clientkit::Interface leftButton1 =
        context.getInterface("/controller/left/1");
    leftButton1.registerCallback(&myButtonCallback, &quit);

    osvr::clientkit::Interface rightButton1 =
        context.getInterface("/controller/right/1");
    rightButton1.registerCallback(&myButtonCallback, &quit);

    // Open OpenGL and set up the context for rendering to
    // an HMD.  Do this using the OSVR RenderManager interface,
    // which maps to the nVidia or other vendor direct mode
    // to reduce the latency.
    osvr::renderkit::RenderManager* render =
        osvr::renderkit::createRenderManager(context.get(), "OpenGL");

    if ((render == nullptr) || (!render->doingOkay())) {
        std::cerr << "Could not create RenderManager" << std::endl;
        return 1;
    }

    // Set callback to handle setting up rendering in an eye
    render->SetViewProjectionCallback(SetupEye);

    // Set callback to handle setting up rendering in a display
    render->SetDisplayCallback(SetupDisplay);

    // Register callbacks to render things in left hand, right
    // hand, and world space.
    render->AddRenderCallback("/", DrawWorld);
    render->AddRenderCallback("/me/hands/left", DrawHand);
    render->AddRenderCallback("/me/hands/right", DrawHand);

// Set up a handler to cause us to exit cleanly.
#ifdef _WIN32
    SetConsoleCtrlHandler((PHANDLER_ROUTINE)CtrlHandler, TRUE);
#endif

    // Open the display and make sure this worked.
    osvr::renderkit::RenderManager::OpenResults ret = render->OpenDisplay();
    if (ret.status == osvr::renderkit::RenderManager::OpenStatus::FAILURE) {
        std::cerr << "Could not open display" << std::endl;
        delete render;
        return 2;
    }
    if (ret.library.OpenGL == nullptr) {
        std::cerr << "Attempted to run an OpenGL program with a config file "
                  << "that specified a different rendering library."
                  << std::endl;
        return 3;
    }

    // Set up the rendering state we need.
    if (!SetupRendering(ret.library)) {
        return 3;
    }

    glewExperimental = true;
    if (glewInit() != GLEW_OK) {
        std::cerr << "Failed ot initialize GLEW\n" << std::endl;
        return -1;
    }
    // Clear any GL error that Glew caused.  Apparently on Non-Windows
    // platforms, this can cause a spurious  error 1280.
    glGetError();

    // Initialize Freetype and load the font we're going to use.
    // This must be done after GLEW and OpenGL are initialized.
    if (FT_Init_FreeType(&g_ft)) {
      std::cerr << "Could not init freetype library" << std::endl;
    } else {
      // Check for any available fonts.  Use the first one we find.
      bool found = false;
      for (auto f : FONTS) {
        if (0 == FT_New_Face(g_ft, f, 0, &g_face)) {
          found = true;
        } else {
          std::cerr << "Fovea: Could not open font " << f << std::endl;
        }
      }
      if (!found) {
        std::cerr << "Fovea: Could not open any font" << std::endl;
        FT_Done_FreeType(g_ft);
        g_ft = nullptr;
      } else {
        FT_Set_Pixel_Sizes(g_face, 0, FONT_SIZE);
      }
    }
    glGenBuffers(1, &g_fontVertexBuffer);

    // Continue rendering until it is time to quit.
    while (!quit) {
        // Update the context so we get our callbacks called and
        // update tracker state.
        context.update();

        if (!render->Render()) {
            std::cerr
                << "Render() returned false, maybe because it was asked to quit"
                << std::endl;
            quit = true;
        }
    }

    if (g_face) { FT_Done_Face(g_face); g_face = nullptr; }
    if (g_ft) { FT_Done_FreeType(g_ft); g_ft = nullptr; }

    // Close the Renderer interface cleanly.
    delete render;

    return 0;
}
