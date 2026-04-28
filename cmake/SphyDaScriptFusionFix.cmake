# Without editing thirdparty/dascript: libDaScript's SETUP_LIB_DEFINITIONS sets
# DAS_FUSION=1/2 for non-Debug, while Debug uses FUSION=0. That can make
# Release-only simulate/relocate + SimNode paths differ from Debug.
#
# Undefine then set DAS_FUSION=0 on libDaScript and on game targets so the
# driver sees 0 after the thirdparty -D (GCC/Clang: last -D wins).

# function(sphy_das_fusion_zero_release)
#   foreach(t IN LISTS ARGN)
#     if(TARGET ${t})
#       if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
#         target_compile_options(
#           ${t}
#           PRIVATE
#             "$<$<OR:$<CONFIG:Release>,$<CONFIG:MinSizeRel>,$<CONFIG:RelWithDebInfo>>:SHELL:-U DAS_FUSION -D DAS_FUSION=0>"
#         )
#       elseif(MSVC)
#         target_compile_options(
#           ${t}
#           PRIVATE
#             "$<$<OR:$<CONFIG:Release>,$<CONFIG:MinSizeRel>,$<CONFIG:RelWithDebInfo>>:/D DAS_FUSION=0>"
#         )
#       else()
#         target_compile_options(
#           ${t}
#           PRIVATE
#             "$<$<OR:$<CONFIG:Release>,$<CONFIG:MinSizeRel>,$<CONFIG:RelWithDebInfo>>:-D DAS_FUSION=0>"
#         )
#       endif()
#     endif()
#   endforeach()
# endfunction()
