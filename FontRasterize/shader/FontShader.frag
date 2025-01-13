//ÑÕÉ«×ÅÉ«Æ÷
#version 460 core
out vec4 frag_color;
in vec2 texc;
in vec4 color;
uniform sampler2D FontAtlasTexture;
void  main()
{
    frag_color=color*texture(FontAtlasTexture, texc);
}