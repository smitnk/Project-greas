# ADAPTED for OpenGL ES without GL_EXT_shader_implicit_conversions (missing on ARM Mali): the int
# literals Blender 3.6.23's GLSL uses in float expressions become float literals. Applied to the
# generated shaders by run_gles_gen.sh; glslang checks every stage compiles as plain ESSL 3.20.
s/textureLod(\([^;]*\), 0)\([.;]\)/textureLod(\1, 0.0)\2/g
s/return (v >= 0) ? res : M_PI - res;/return (v >= 0.0) ? res : M_PI - res;/
s/(v\.\([xyzw]\) >= 0)/(v.\1 >= 0.0)/g
s/(ma\.x == -1 \&\& x == -1)/(ma.x == -1 \&\& x == -1.0)/
s/(ma3\.x == -1 \&\& x == 1)/(ma3.x == -1 \&\& x == 1.0)/
s/    float fill_opacity = fcol1\.a - (fcol_decode\.a \* 10);/    float fill_opacity = fcol1.a - (fcol_decode.a * 10.0);/
