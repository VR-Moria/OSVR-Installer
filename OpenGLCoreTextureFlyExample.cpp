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
#include <osvr/ClientKit/Context.h>
#include <osvr/ClientKit/Interface.h>
#include <osvr/ClientKit/InterfaceStateC.h>
#include <osvr/RenderKit/RenderManager.h>
#include <quat.h>
#include <chrono>

// Library/third-party includes
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#include <GL/glew.h>

#include <freetype2/ft2build.h>
#include FT_FREETYPE_H

#include <fstream>  //for parsing text file
// Standard includes
#include <iostream>
#include <string>
#include <stdlib.h> // For exit()

// This must come after we include <GL/gl.h> so its pointer types are defined.
#include <osvr/RenderKit/GraphicsLibraryOpenGL.h>
#include <osvr/RenderKit/RenderKitGraphicsTransforms.h>

///
// normally you'd load the shaders from a file, but in this case, let's
// just keep things simple and load from memory.

/// @brief This is the OpenGL shader used to transform vertices and send parameters
///         to the fragment shader.
/// @param [in] position The 3D coordinate of the vertex
/// @param [in] vertexColor The color of the vertex (red, green, blue, alpha)
/// @param [in] vertexTextureCoord Normalized 2D texture coordinates between 0 and 1
///             This is used to render text onto shapes, but could also be used to
///             render other textures.
/// @param [out] fragmentColor The color of the fragment, passed through and interpolated
/// @param [out] textureCoord The texture coordinates, passed through and interpolated
static const GLchar* vertexShader =
    "#version 330 core\n"
    "layout(location = 0) in vec3 position;\n"
    "layout(location = 1) in vec4 vertexColor;\n"
    "layout(location = 2) in vec2 vertexTextureCoord;\n"
    "out vec4 fragmentColor;\n"
    "out vec2 textureCoord;\n"
    "uniform mat4 modelView;\n"
    "uniform mat4 projection;\n"
    "void main()\n"
    "{\n"
    "   gl_Position = projection * modelView * vec4(position,1);\n"
    "   fragmentColor = vertexColor;\n"
    "   textureCoord = vertexTextureCoord;\n"
    "}\n";


static const int XY = 0;
static const int XZ = 1;
static const int YZ = 2;

/// @brief This is the OpenGL shader used to color fragments.
/// @param [in] tex The texture sampler used to map the texture.  The texture value
///             multiplied by the fragment color, and alpha is supported, so that
///             the texture can recolor the fragment and also change its opacity.
static const GLchar* fragmentShader =
    "#version 330 core\n"
    "in vec4 fragmentColor;\n"
    "in vec2 textureCoord;\n"
    "layout(location = 0) out vec4 color;\n"
    "uniform sampler2D tex;\n"
    "void main()\n"
    "{\n"
    "   color = fragmentColor * texture(tex, textureCoord);\n"
    "}\n";

/// @brief Class that wraps all of the things needed to handle OpenGL vertex and fragment shaders.
///
/// This class handles compiling and linking the shaders, passing parameters to them, and
/// making them active for rendering.
class SampleShader {
  public:
    /// @brief Constructor must be called after OpenGL is initialized.
    SampleShader() {}

    ~SampleShader() {
        if (initialized) {
            glDeleteProgram(programId);
        }
    }

    /// @brief Must be called before useProgram() is called to initialize the shaders.
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

    /// @brief Makes the shader active so that the following OpenGL render calls will use it.
    /// @param [in] projection OpenGL projection matrix to use.  This should be obtained
    ///             from OSVR.
    /// @param [in] modelView OpenGL model/view matrix to use.  This should be obtained
    ///             from OSVR.
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
std::vector<const char*> FONTS = {"./COURIER.TTF"};
const int FONT_SIZE = 48;
GLuint g_font_tex = 0;
GLuint g_on_tex = 0;
GLuint g_fontShader = 0;
GLuint g_fontVertexBuffer = 0;
GLuint g_fontVertexArrayId = 0;

/// @brief Structure to hold OpenGL vertex buffer data.
class FontVertex {
public:
  GLfloat pos[3];   ///< Location of the vertexre
  GLfloat col[4];   ///< Color of the vertex (red, green, blue, alpha)
  GLfloat tex[2];   ///< Texture coordinates for the vertex
};

/// @brief Helper function for render_text.
/// @param [out] vertexBufferData OpenGL vertex buffer data to be rendered
///             for a single character of text onto a quadrilateral.
///
///   The quadrilateral will be in the X-Y plane whose depth is specified.
/// It will be axis aligned, with the character reading towards +X with
/// its top rendered towards +Y.
///   The quadrilateral is rendered into whatever space is defined by
/// the projection and model/view transforms being used by the shader.
///
/// @param [in] left Furthest left (-x) on the Quadrilateral
/// @param [in] right Furthest right (+x) on the Quadrilateral
/// @param [in] left Furthest up (+y) on the Quadrilateral
/// @param [in] left Furthest down (-y) on the Quadrilateral
/// @param [in] depth Z value of the quadrilateral.
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

static void addFontQuadXZ(std::vector<FontVertex>& vertexBufferData,
    GLfloat left, GLfloat right, GLfloat y, GLfloat maxZ, GLfloat minZ,
    GLfloat R, GLfloat G, GLfloat B, GLfloat alpha)
{
    FontVertex v;
    v.col[0] = R; v.col[1] = G; v.col[2] = B; v.col[3] = alpha;

    // Invert the Y texture coordinate so that we draw the textures
    // right-side up.
    // Switch the order so we have clockwise front-facing.
    v.pos[0] = left; v.pos[1] = y; v.pos[2] = minZ;
    v.tex[0] = 0; v.tex[1] = 1;
    vertexBufferData.emplace_back(v);
    v.pos[0] = right; v.pos[1] = y; v.pos[2] = maxZ;
    v.tex[0] = 1; v.tex[1] = 0;
    vertexBufferData.emplace_back(v);
    v.pos[0] = right; v.pos[1] = y; v.pos[2] = minZ;
    v.tex[0] = 1; v.tex[1] = 1;
    vertexBufferData.emplace_back(v);

    v.pos[0] = left; v.pos[1] = y; v.pos[2] = minZ;
    v.tex[0] = 0; v.tex[1] = 1;
    vertexBufferData.emplace_back(v);
    v.pos[0] = left; v.pos[1] = y; v.pos[2] = maxZ;
    v.tex[0] = 0; v.tex[1] = 0;
    vertexBufferData.emplace_back(v);
    v.pos[0] = right; v.pos[1] = y; v.pos[2] = maxZ;
    v.tex[0] = 1; v.tex[1] = 0;
    vertexBufferData.emplace_back(v);
}

static void addFontQuadYZ(std::vector<FontVertex>& vertexBufferData,
    GLfloat x, GLfloat top, GLfloat bot, GLfloat maxZ, GLfloat minZ,
    GLfloat R, GLfloat G, GLfloat B, GLfloat alpha)
{
    FontVertex v;
    v.col[0] = R; v.col[1] = G; v.col[2] = B; v.col[3] = alpha;

    // Invert the Y texture coordinate so that we draw the textures
    // right-side up.
    // Switch the order so we have clockwise front-facing.
    v.pos[0] = x; v.pos[1] = bot; v.pos[2] = minZ;
    v.tex[0] = 0; v.tex[1] = 1;
    vertexBufferData.emplace_back(v);
    v.pos[0] = x; v.pos[1] = top; v.pos[2] = maxZ;
    v.tex[0] = 1; v.tex[1] = 0;
    vertexBufferData.emplace_back(v);
    v.pos[0] = x; v.pos[1] = bot; v.pos[2] = maxZ;
    v.tex[0] = 1; v.tex[1] = 1;
    vertexBufferData.emplace_back(v);

    v.pos[0] = x; v.pos[1] = bot; v.pos[2] = minZ;
    v.tex[0] = 0; v.tex[1] = 1;
    vertexBufferData.emplace_back(v);
    v.pos[0] = x; v.pos[1] = top; v.pos[2] = minZ;
    v.tex[0] = 0; v.tex[1] = 0;
    vertexBufferData.emplace_back(v);
    v.pos[0] = x; v.pos[1] = top; v.pos[2] = maxZ;
    v.tex[0] = 1; v.tex[1] = 0;
    vertexBufferData.emplace_back(v);
}

/// @brief Render a string of text into a specified space.
///
///   The text will be in the X-Y plane whose z value is specified.
/// It will be axis aligned, with the text reading towards +X with
/// its top rendered towards +Y.
///   The quadrilateral is rendered into whatever space is defined by
/// the projection and model/view transforms being used by the shader.
///
/// @param [in] projection The OpenGL projection matrix to pass to the shader.
/// @param [in] modelView The OpenGL model/view matrix to pass to the shader.
/// @param [in] text The null-terminated string of text to be rendered.
/// @param [in] x Coordinates of the center of the text.
/// @param [in] y Coordinates of the center of the text.
/// @param [in] z Coordinates of the center of the text.
/// @param [in] sx Spacing for the text in x.
/// @param [in] sy Spacing for the text in y.

bool render_text(const GLdouble projection[], const GLdouble modelView[],
    const char *text, float x, float y, float z, float sx, float sy, int plane)
{
  if (!g_face) {
    std::cerr << "render_text(): No face" << std::endl;
    return false;
  }
  FT_GlyphSlot g = g_face->glyph;
  GLenum err;

  // Use the font shader to render this.  It may activate a different texture unit, so we
  // need to make sure we active the first one once we are using the program.
  sampleShader.useProgram(projection, modelView);

  err = glGetError();
  if (err != GL_NO_ERROR) {
    std::cerr << "render_text(): Error after use program: "
      << err << std::endl;
    return false;
  }

  glActiveTexture(GL_TEXTURE0);

  err = glGetError();
  if (err != GL_NO_ERROR) {
    std::cerr << "render_text(): Error after texture set: "
      << err << std::endl;
    return false;
  }

  std::vector<FontVertex> vertexBufferData;

  // Enable blending using alpha.
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_COLOR, GL_ONE_MINUS_SRC_ALPHA);

  err = glGetError();
  if (err != GL_NO_ERROR) {
    std::cerr << "render_text(): Error after blend enable: "
      << err << std::endl;
    return false;
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
    return false;
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
      return false;
    }
#ifdef __APPLE__
    // Trying to send GL_LUMINANCE textures fails on the mac, though it
    // work on Windows.  So to fix this, we make a 4-times copy of the
    // data and then send that as RGBA.
    std::vector<GLubyte> tex(4 * g->bitmap.width * g->bitmap.rows);
    for (int r = 0; r < g->bitmap.rows; r++) {
      for (int c = 0; c < g->bitmap.width; c++) {
        GLubyte val = g->bitmap.buffer[c + g->bitmap.width * r];
        for (int i = 0; i < 4; i++) {
          tex[i + 4 * (c + (g->bitmap.width * r))] = val;
        }
      }
    }
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, g->bitmap.width, g->bitmap.rows,
      0, GL_RGBA, GL_UNSIGNED_BYTE, tex.data());
#else
    glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, g->bitmap.width, g->bitmap.rows,
      0, GL_LUMINANCE, GL_UNSIGNED_BYTE, g->bitmap.buffer);
#endif
    err = glGetError();
    if (err != GL_NO_ERROR) {
      std::cerr << "render_text(): Error writing texture: "
        << err << std::endl;
      return false;
    }

    float x2; float y2; float z2; float w; float h;
    
    switch (plane) 
    case 0: { //xy
         x2 = x + g->bitmap_left * sx;
         y2 = y + g->bitmap_top * sy;
         w = g->bitmap.width * sx;
         h = g->bitmap.rows * sy;
        // Blend in the text, fully opaque (inverse alpha) and fully white.
        vertexBufferData.clear();
        addFontQuad(vertexBufferData, x2, x2 + w, y2, y2 - h, z, 1, 1, 1, 0);
        break;
    case 1:  //xz
         x2 = x + g->bitmap_left * sx;
         z2 = z + g->bitmap_top * sy;
         w = g->bitmap.width * sx;
         h = g->bitmap.rows * sy;
        // Blend in the text, fully opaque (inverse alpha) and fully white.
        vertexBufferData.clear();
        addFontQuadXZ(vertexBufferData, x2, x2 + w, y, z2-h, z2, 1, 1, 1, 0);
        break;
    
    case 2:  //yz
         z2 = z + g->bitmap_left * sx;
         y2 = y + g->bitmap_top * sy;
         w = g->bitmap.width * sx;
         h = g->bitmap.rows * sy;
        // Blend in the text, fully opaque (inverse alpha) and fully white.
        vertexBufferData.clear();
        addFontQuadYZ(vertexBufferData, x, y2, y2 - h, z2, z2+w, 1, 1, 1, 0);
        break;
    
    }

     // Blend in the text, fully opaque (inverse alpha) and fully white.
    //vertexBufferData.clear();
    //addFontQuad(vertexBufferData, x2, x2 + w, y2, y2 - h, z, 1,1,1,0);
    //addFontQuadXZ(vertexBufferData, x2, x2+w, y, z2, z2-h, 1, 1, 1, 0);

    glBindBuffer(GL_ARRAY_BUFFER, g_fontVertexBuffer);
    glBufferData(GL_ARRAY_BUFFER,
      sizeof(vertexBufferData[0]) * vertexBufferData.size(),
      &vertexBufferData[0], GL_STATIC_DRAW);
    err = glGetError();
    if (err != GL_NO_ERROR) {
      std::cerr << "render_text(): Error buffering data: "
        << err << std::endl;
      return false;
    }

    // Configure the vertex-buffer objects.
    glBindBuffer(GL_ARRAY_BUFFER, g_fontVertexBuffer);
    glBindVertexArray(g_fontVertexArrayId);
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
    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    x += (g->advance.x / 64) * sx;
    y += (g->advance.y / 64) * sy;
  }

  // Set things back to the defaults
  glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
  glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);

  // Use the always-on texture when we're not rendering text.
  glBindTexture(GL_TEXTURE_2D, g_on_tex);
  glDisable(GL_BLEND);

  return true;
}

/// @brief Class to handle creating and rendering a cube in OpenGL.
class Cube {
  public:
    /// @brief Constructor for the cube.  Must be called after OpenGL is initialized.
    /// @param [in] scale Size of one face of the cube in meters.
    Cube(GLfloat scale) {
      /// @brief Colors for each vertex in the cube
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

      /// @brief Locations for each vertex in the cube
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

    /// @brief Must be called before draw() can be used.
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

    /// @brief Render the cube in the specified space.
    /// @param [in] projection The OpenGL projection matrix to pass to the shader.
    /// @param [in] modelView The OpenGL model/view matrix to pass to the shader.
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

/// @brief Callback to draw things in world space.
///
/// Edit this function to draw things in the world, which will remain in place
/// while the viewpoint is changed and the user flies around the world.
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
    glBindTexture(GL_TEXTURE_2D, g_on_tex);
    //roomCube.draw(projectionGL, viewGL);

    //open text file and parse one char at a time, 
    //carriage return at every newline
    std::ifstream ifs;
    // std::cerr << "attempting to open text file\n";
    ifs.open ("../../../UBuild/umoria/print_floor_test.txt", std::ifstream::in);
    if (ifs.is_open()) {
        // std::cerr << "opened file\n";
    } else {
        std::cerr << "could not open file\n";
        perror("file.txt ");  
        exit(1);
    }

    char c;
    GLuint playerX = 0;
    GLuint playerZ = 0;
    while (ifs.get(c)) {  //get coords for @
        char curr = c;
        if (curr == '@') {
            break;
        } 
        else if (curr == '\n') {
            playerX -= 4;
            playerZ = 0;
        } else {
            playerZ += 4;
        }
    }
    //translate around @
    std::cerr << "translating X:";
    std::cerr << playerX;
    std::cerr << "\n";
    std::cerr << "translating Z:";
    std::cerr << playerZ;
    std::cerr << "\n";

    GLuint dx = playerX;
    GLuint dz = playerZ;

    while (ifs.get(c)) {         // loop rendering characters
        // std::cerr << c;
        char curr = c;
        if (curr != '\n') {
          char* arr = new char[2];
          arr[0] = curr;
          arr[1] = '\0';
          if (!render_text(projectionGL, viewGL, arr, dx,-2,dz, 0.1f, 0.1f, XZ)) {
            quit = true;
          }
          dz -= 4;
        }
        else {
            dz = playerZ;
            dx += 4;
        }     
    }
    ifs.close();                // close file

    // if (!render_text(projectionGL, viewGL, "#", -1,-2,0, 0.1f, 0.1f, XZ)) {
    //   quit = true;
    // }
    // if (!render_text(projectionGL, viewGL, "#", -1,-2,-5, 0.1f, 0.1f, XZ)) {
    //   quit = true;
    // }
    // if (!render_text(projectionGL, viewGL, "#", -1,-2,-2, 0.1f, 0.1f, XY)) {
    //   quit = true;
    // }
}





/// @brief Callback to draw things in head space.
///
// This can be used to draw a heads-up display.  Unlike in a non-VR game,
// this can't be drawn in screen space because it has to be at a consistent
// location for stereo viewing through potentially-distorted and offset lenses
// from the HMD.  This example uses a small cube drawn in front of us.
// NOTE: For a fixed-display set-up, you do want to draw in screen space.
void DrawHead(
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

  // Draw some text in front of us.
  glBindTexture(GL_TEXTURE_2D, g_on_tex);
  // if (!render_text(projectionGL, viewGL, "Hello, Head Space", -1,0,-2, 0.003f,0.003f, XY)) {
  //   quit = true;
  // }
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
    glBindTexture(GL_TEXTURE_2D, g_on_tex);
    handsCube.draw(projectionGL, viewGL);
}

static void q_from_OSVR(q_xyz_quat_type& xform, const OSVR_PoseState& pose)
{
  xform.xyz[Q_X] = pose.translation.data[0];
  xform.xyz[Q_Y] = pose.translation.data[1];
  xform.xyz[Q_Z] = pose.translation.data[2];
  xform.quat[Q_X] = osvrQuatGetX(&pose.rotation);
  xform.quat[Q_Y] = osvrQuatGetY(&pose.rotation);
  xform.quat[Q_Z] = osvrQuatGetZ(&pose.rotation);
  xform.quat[Q_W] = osvrQuatGetW(&pose.rotation);
}

static void q_to_OSVR(OSVR_PoseState& pose, const q_xyz_quat_type& xform)
{
  pose.translation.data[0] = xform.xyz[Q_X];
  pose.translation.data[1] = xform.xyz[Q_Y];
  pose.translation.data[2] = xform.xyz[Q_Z];
  osvrQuatSetX(&pose.rotation, xform.quat[Q_X]);
  osvrQuatSetY(&pose.rotation, xform.quat[Q_Y]);
  osvrQuatSetZ(&pose.rotation, xform.quat[Q_Z]);
  osvrQuatSetW(&pose.rotation, xform.quat[Q_W]);
}

int main(int argc, char* argv[])
{
    GLenum err;

    // Get an OSVR client context to use to access the devices
    // that we need.
    osvr::clientkit::ClientContext context(
        "com.reliasolve.OSVR-Installer.OpenGLCoreTextureFlyExample");

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

    // Construct the analog devices we need to read info
    // needed for flying.
    osvr::clientkit::Interface analogTrigger =
      context.getInterface("/controller/trigger");
    osvr::clientkit::Interface analogLeftStickX =
      context.getInterface("/controller/leftStickX");
    osvr::clientkit::Interface analogLeftStickY =
      context.getInterface("/controller/leftStickY");
    osvr::clientkit::Interface analogRightStickX =
      context.getInterface("/controller/rightStickX");
    osvr::clientkit::Interface headSpace =
      context.getInterface("/me/head");

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
    render->AddRenderCallback("/me/head", DrawHead);
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
    glGenVertexArrays(1, &g_fontVertexArrayId);

    // Make an all-on texture to use when we're not rendering text.  Fill it with all 1's.
    // Note: We could also use a different shader for when we're not rendering textures.
    // Set the parameters we need to render the text properly.
    // Leave this texture bound whenever we're not drawing text.
    glGenTextures(1, &g_on_tex);
    glBindTexture(GL_TEXTURE_2D, g_on_tex);
    GLubyte onTex[] = {
      255,255,255,255,
      255,255,255,255,
      255,255,255,255,
      255,255,255,255 };
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, 2);
    err = glGetError();
    if (err != GL_NO_ERROR) {
      std::cerr << "Error setting parameters for 'on' texture: "
        << err << std::endl;
    }
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 2, 2,
      0, GL_RGBA, GL_UNSIGNED_BYTE, onTex);
    err = glGetError();
    if (err != GL_NO_ERROR) {
      std::cerr << "Error writing 'on' texture: "
        << err << std::endl;
      quit = true;
    }

    // Set up a world-from-room additional transformation that we will
    // adjust as the user flies around using a joystick.  They always fly
    // in the local viewing coordinate system.
    OSVR_PoseState pose;
    osvrPose3SetIdentity(&pose);

#if 0
    // Set up any initial offset and orient changes that we want.
    double X_OFFSET = -48;  //-29
    double Y_OFFSET = 20;   // 58
    double Z_OFFSET = 49.5; // 49
    /*XXX
    double X_OFFSET = 0;  //-29
    double Y_OFFSET = 3;   // 58
    double Z_OFFSET = 5; // 49
    */
    pose.translation.data[0] = X_OFFSET;
    pose.translation.data[1] = Y_OFFSET;
    pose.translation.data[2] = Z_OFFSET;
    // Rotate by 90 degrees around X to make Z down
    pose.rotation.data[0] = 0.7071;
    pose.rotation.data[1] = 0.7071;
    q_type initialRotation;
    initialRotation[Q_X] = osvrQuatGetX(&pose.rotation);
    initialRotation[Q_Y] = osvrQuatGetY(&pose.rotation);
    initialRotation[Q_Z] = osvrQuatGetZ(&pose.rotation);
    initialRotation[Q_W] = osvrQuatGetW(&pose.rotation);
#endif

    // Keeps track of when we last updated the system
    // state.
    OSVR_TimeValue  lastTime;
    osvrTimeValueGetNow(&lastTime);

    // Continue rendering until it is time to quit.
    while (!quit) {
        // Update the context so we get our callbacks called and
        // update tracker state.
        context.update();

        //==========================================================================
        // This section handles flying the user around based on the analog inputs.

        // Read the current value of the analogs we want
        OSVR_TimeValue  ignore;
        OSVR_AnalogState triggerValue = 0;
        osvrGetAnalogState(analogTrigger.get(), &ignore, &triggerValue);
        OSVR_AnalogState leftStickXValue = 0;
        osvrGetAnalogState(analogLeftStickX.get(), &ignore, &leftStickXValue);
        OSVR_AnalogState leftStickYValue = 0;
        osvrGetAnalogState(analogLeftStickY.get(), &ignore, &leftStickYValue);
        OSVR_AnalogState rightStickXValue = 0;
        osvrGetAnalogState(analogRightStickX.get(), &ignore, &rightStickXValue);

        // Figure out how much to move and in which directions based
        // on how much time as passed and what the analog values are.
        const double X_SPEED_SCALE = 3.0;
        const double Y_SPEED_SCALE = -3.0;  // Y axis on controller is backwards
        const double Z_SPEED_SCALE = -2.0;
        const double SPIN_SPEED_SCALE = -Q_PI / 2;  // Want to spin the other way
        OSVR_TimeValue  now;
        osvrTimeValueGetNow(&now);
        OSVR_TimeValue nowCopy = now;
        osvrTimeValueDifference(&now, &lastTime);
        lastTime = nowCopy;

        double dt = now.seconds + now.microseconds * 1e-6;
        double right = dt * leftStickXValue * X_SPEED_SCALE;
        double forward = dt * leftStickYValue * Y_SPEED_SCALE;
        double up = dt * triggerValue * Z_SPEED_SCALE;
        double spin = dt * rightStickXValue * SPIN_SPEED_SCALE;

        // The deltaY will always point up in world space, but the
        // motion in X and Y need to be rotated so that X goes in the
        // direction of forward gaze (-Z) and Y goes to the right (X).
        // These will be arbitrary 3D locations, so will be added to
        // all of X, Y, and Z.
        double deltaX = 0, deltaY = 0, deltaZ = 0;
        deltaY += up;

        // Make forward be along -Z in head space.
        // Remember that room space is rotated w.r.t. world space
        OSVR_PoseState currentHead;
        OSVR_ReturnCode ret = osvrGetPoseState(headSpace.get(), &ignore, &currentHead);
        if (ret == OSVR_RETURN_SUCCESS) {

          // Adjust the rotation by spinning around the vertical (Y) axis
          q_type rot;
          q_from_axis_angle(rot, 0, 1, 0, spin);
          q_xyz_quat_type cur_pose;
          q_from_OSVR(cur_pose, pose);
          q_mult(cur_pose.quat, rot, cur_pose.quat);
          q_to_OSVR(pose, cur_pose);

          // Get the current head pose in room space.
          q_xyz_quat_type poseXform;
          q_from_OSVR(poseXform, currentHead);

          // Find -Z in world space by catenating the room-to-world
          // rotation.
          q_vec_type negZ = { 0, 0, -1 };
          q_vec_type forwardDir;
          q_xform(forwardDir, poseXform.quat, negZ);
          q_xform(forwardDir, cur_pose.quat, forwardDir);
          q_vec_scale(forwardDir, forward, forwardDir);
          deltaX += forwardDir[Q_X];
          deltaY += forwardDir[Q_Y];
          deltaZ += forwardDir[Q_Z];

          // Make right be along +X in head space.
          // Remember that room space is rotated w.r.t. world space.
          q_vec_type X = { 1, 0, 0 };
          q_vec_type rightDir;
          q_xform(rightDir, poseXform.quat, X);
          q_xform(rightDir, cur_pose.quat, rightDir);
          q_vec_scale(rightDir, right, rightDir);
          deltaX += rightDir[Q_X];
          deltaY += rightDir[Q_Y];
          deltaZ += rightDir[Q_Z];

          // Adjust the roomToWorld pose based on the changes,
          // unless there was too long of a time between readings.
          if (dt < 0.5) {
            pose.translation.data[0] += deltaX;
            pose.translation.data[1] += deltaY;
            pose.translation.data[2] += deltaZ;
          }
        }

        //==========================================================================
        // Render the scene, sending it the current roomToWorld transform that
        // tells it about how we are flying.
        osvr::renderkit::RenderManager::RenderParams params;
        params.worldFromRoomAppend = &pose;
        if (!render->Render(params)) {
            std::cerr
                << "Render() returned false, maybe because it was asked to quit"
                << std::endl;
            quit = true;
        }
    }

    glDeleteVertexArrays(1, &g_fontVertexArrayId);
    glDeleteBuffers(1, &g_fontVertexBuffer);
    glDeleteTextures(1, &g_on_tex);
    glDeleteTextures(1, &g_font_tex);
    if (g_face) { FT_Done_Face(g_face); g_face = nullptr; }
    if (g_ft) { FT_Done_FreeType(g_ft); g_ft = nullptr; }

    // Close the Renderer interface cleanly.
    delete render;

    return 0;
}
