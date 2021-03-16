#
# library_regular_naming.cmake
#
# 2020-6-22 By GuoJH

# 平台工具集
function(get_toolset toolset)
    # https://docs.microsoft.com/en-us/cpp/preprocessor/predefined-macros?view=vs-2019
    
    set(MSToolSet "vc142")
    if(MSVC_VERSION LESS 1300)
        set(MSToolSet "vc6")
    elseif(MSVC_VERSION LESS 1310)
        set(MSToolSet "vc70")
    elseif(MSVC_VERSION LESS 1400)
        set(MSToolSet "vc71")
    elseif(MSVC_VERSION LESS 1500)
        set(MSToolSet "vc80")
    elseif(MSVC_VERSION LESS 1600)
        set(MSToolSet "vc90")
    elseif(MSVC_VERSION LESS 1700)
        set(MSToolSet "vc100")
    elseif(MSVC_VERSION LESS 1800)
        set(MSToolSet "vc110")
    elseif(MSVC_VERSION LESS 1900)
        set(MSToolSet "vc120")
    elseif(MSVC_VERSION LESS 1910)
        set(MSToolSet "vc140")
    elseif(MSVC_VERSION LESS 1920)
        set(MSToolSet "vc141")
    else()
        set(MSToolSet "vc142")
    endif()

    set(${toolset} ${MSToolSet} PARENT_SCOPE)

    # message(STATUS "**> MSVC_VERSION=${MSVC_VERSION}")
    # message(STATUS "**> MSToolSet=${MSToolSet}")
endfunction()

# 运行时
function(get_runtime c_or_cxx_flags is_mt)
    string(FIND ${c_or_cxx_flags} "MD" md_pos)
    string(FIND ${c_or_cxx_flags} "MT" mt_pos)

    # message(STATUS "**> md_pos: ${md_pos} ${c_or_cxx_flags}")
    # message(STATUS "**> mt_pos: ${mt_pos}")

    if(${mt_pos} EQUAL -1)
        set(${is_mt} OFF PARENT_SCOPE)
    elseif(${md_pos} EQUAL -1)
        set(${is_mt} ON PARENT_SCOPE)
    elseif(${md_pos} GREATER ${mt_pos})
        set(${is_mt} OFF PARENT_SCOPE)
    else()
        set(${is_mt} ON PARENT_SCOPE)
    endif()
endfunction()

# 架构标检
function(get_architecture_tag arch_tag)
    #  -x86     Architecture and address model tag
    #           First character is the architecture, then word-size, either 32 or 64
    #           Only used in 'versioned' layout, added in Boost 1.66.0
    set(ARCH_TAG "")
    # {CMAKE_CXX_COMPILER_ARCHITECTURE_ID} is not currently set for all compilers
    if(NOT "x${CMAKE_CXX_COMPILER_ARCHITECTURE_ID}" STREQUAL "x")
      string(APPEND ARCH_TAG "-")
      # This needs to be kept in-sync with the section of CMakePlatformId.h.in
      # inside 'defined(_WIN32) && defined(_MSC_VER)'
      if(${CMAKE_CXX_COMPILER_ARCHITECTURE_ID} STREQUAL "IA64")
        string(APPEND ARCH_TAG "i")
      elseif(${CMAKE_CXX_COMPILER_ARCHITECTURE_ID} STREQUAL "X86"
                OR ${CMAKE_CXX_COMPILER_ARCHITECTURE_ID} STREQUAL "x64")
        string(APPEND ARCH_TAG "x")
      elseif(${CMAKE_CXX_COMPILER_ARCHITECTURE_ID} MATCHES "^ARM")
        string(APPEND ARCH_TAG "a")
      elseif(${CMAKE_CXX_COMPILER_ARCHITECTURE_ID} STREQUAL "MIPS")
        string(APPEND ARCH_TAG "m")
      endif()

      if(CMAKE_SIZEOF_VOID_P EQUAL 8)
        string(APPEND ARCH_TAG "64")
      else()
        string(APPEND ARCH_TAG "32")
      endif()
    endif()

    set(${arch_tag} ${ARCH_TAG} PARENT_SCOPE)
    # message(STATUS "**> ARCH_TAG=${ARCH_TAG}")
    # message(STATUS "**> CMAKE_CXX_COMPILER_ARCHITECTURE_ID=${CMAKE_CXX_COMPILER_ARCHITECTURE_ID}")
endfunction()

function(library_regular_naming_with_name target_name target_origin_name)
    get_target_property(LIBRARY_TARGE_TYPE ${target_name} TYPE)
    if(STATIC_LIBRARY MATCHES ${LIBRARY_TARGE_TYPE})
        set(CONFIG_LIB_PREFIX "lib")
    elseif(SHARED_LIBRARY MATCHES ${LIBRARY_TARGE_TYPE})
        if(MSVC)
            set(CONFIG_LIB_PREFIX "")
        else() # UNIX-like
            set(CONFIG_LIB_PREFIX "lib")
        endif()
    else()
        set(CONFIG_LIB_PREFIX "not-library")
    endif()

    if(NOT CONFIG_LIB_PREFIX MATCHES "not-library")
        if(MSVC)
            get_toolset(CONFIG_LIB_TOOLSET)
            get_architecture_tag(CONFIG_ARCHITECTURE_TAG)

            # message(STATUS "**> CONFIG_LIB_TOOLSET=${CONFIG_LIB_TOOLSET}")
            # message(STATUS "**> CONFIG_ARCHITECTURE_TAG=${CONFIG_ARCHITECTURE_TAG}")

            get_runtime(${CMAKE_CXX_FLAGS_DEBUG}   is_debug_mt)
            get_runtime(${CMAKE_CXX_FLAGS_RELEASE} is_release_mt)

            if(is_debug_mt)
                set(CONFIG_DEBUG_LIB_RT_OPT "-mt-sgd")
            else()
                set(CONFIG_DEBUG_LIB_RT_OPT "-mt-gd")
            endif()

            if(is_release_mt)
                set(CONFIG_RELEASE_LIB_RT_OPT "-mt-s")
            else()
                set(CONFIG_RELEASE_LIB_RT_OPT "-mt")
            endif()

            # libname-vc100-mt-sgd-x32.lib
            set(CONFIG_DEBUG_OUTPUT_NAME "${CONFIG_LIB_PREFIX}${target_origin_name}-${CONFIG_LIB_TOOLSET}${CONFIG_DEBUG_LIB_RT_OPT}${CONFIG_ARCHITECTURE_TAG}")
            set(CONFIG_RELEASE_OUTPUT_NAME "${CONFIG_LIB_PREFIX}${target_origin_name}-${CONFIG_LIB_TOOLSET}${CONFIG_RELEASE_LIB_RT_OPT}${CONFIG_ARCHITECTURE_TAG}")
        
        else() # UNIX-like

            # libname.a 
            # libname.so[.x.y.z]
            set(CONFIG_DEBUG_OUTPUT_NAME "${CONFIG_LIB_PREFIX}${target_origin_name}")
            set(CONFIG_RELEASE_OUTPUT_NAME "${CONFIG_LIB_PREFIX}${target_origin_name}")
        endif()

        message(STATUS "**>  ${target_name} Target CONFIG_DEBUG_OUTPUT_NAME=${CONFIG_DEBUG_OUTPUT_NAME}")
        message(STATUS "**>  ${target_name} Target CONFIG_RELEASE_OUTPUT_NAME=${CONFIG_RELEASE_OUTPUT_NAME}")

        # static
        set_target_properties(${target_name} PROPERTIES 
            ARCHIVE_OUTPUT_NAME_DEBUG "${CONFIG_DEBUG_OUTPUT_NAME}"
            ARCHIVE_OUTPUT_NAME_RELEASE "${CONFIG_RELEASE_OUTPUT_NAME}"
            ARCHIVE_OUTPUT_NAME_MINSIZEREL "${CONFIG_RELEASE_OUTPUT_NAME}"
            ARCHIVE_OUTPUT_NAME_RELWITHDEBINFO "${CONFIG_RELEASE_OUTPUT_NAME}")

        # shared
        if(NOT CONFIG_LIB_PREFIX MATCHES "lib")
            set_target_properties(${target_name} PROPERTIES 
            RUNTIME_OUTPUT_NAME_DEBUG "${CONFIG_DEBUG_OUTPUT_NAME}"
            RUNTIME_OUTPUT_NAME_RELEASE "${CONFIG_RELEASE_OUTPUT_NAME}"
            RUNTIME_OUTPUT_NAME_MINSIZEREL "${CONFIG_RELEASE_OUTPUT_NAME}"
            RUNTIME_OUTPUT_NAME_RELWITHDEBINFO "${CONFIG_RELEASE_OUTPUT_NAME}")
        endif()

        # pdb only msvc
        if(MSVC)
            set_target_properties(${target_name} PROPERTIES 
              PDB_NAME_DEBUG "${CONFIG_DEBUG_OUTPUT_NAME}"
              PDB_NAME_RELEASE "${CONFIG_RELEASE_OUTPUT_NAME}"
              PDB_NAME_MINSIZEREL "${CONFIG_RELEASE_OUTPUT_NAME}"
              PDB_NAME_RELWITHDEBINFO "${CONFIG_RELEASE_OUTPUT_NAME}"
              COMPILE_PDB_NAME_DEBUG "${CONFIG_DEBUG_OUTPUT_NAME}"
              COMPILE_PDB_NAME_RELEASE "${CONFIG_RELEASE_OUTPUT_NAME}"
              COMPILE_PDB_NAME_MINSIZERE "${CONFIG_RELEASE_OUTPUT_NAME}"
              COMPILE_PDB_NAME_RELWITHDEBINFO "${CONFIG_RELEASE_OUTPUT_NAME}")
        endif()
    endif()
endfunction()

function(library_regular_naming target_name)
    get_target_property(target_origin_name ${target_name} NAME)
    library_regular_naming_with_name(${target_name} ${target_origin_name})
endfunction()

function(library_regular_named_query library_debug library_release library_type orgin)
    if(MSVC)
        get_toolset(CONFIG_LIB_TOOLSET)
        get_architecture_tag(CONFIG_ARCHITECTURE_TAG)

        get_runtime(${CMAKE_CXX_FLAGS_DEBUG}   is_debug_mt)
        get_runtime(${CMAKE_CXX_FLAGS_RELEASE} is_release_mt)

        if(is_debug_mt)
            set(CONFIG_DEBUG_LIB_RT_OPT "-mt-sgd")
        else()
            set(CONFIG_DEBUG_LIB_RT_OPT "-mt-gd")
        endif()

        if(is_release_mt)
            set(CONFIG_RELEASE_LIB_RT_OPT "-mt-s")
        else()
            set(CONFIG_RELEASE_LIB_RT_OPT "-mt")
        endif()

        set(CONFIG_PALTFORM_EXTENSION ".lib")

        set(${library_debug} "${orgin}-${CONFIG_LIB_TOOLSET}${CONFIG_DEBUG_LIB_RT_OPT}${CONFIG_ARCHITECTURE_TAG}${CONFIG_PALTFORM_EXTENSION}" PARENT_SCOPE)
        set(${library_release} "${orgin}-${CONFIG_LIB_TOOLSET}${CONFIG_RELEASE_LIB_RT_OPT}${CONFIG_ARCHITECTURE_TAG}${CONFIG_PALTFORM_EXTENSION}" PARENT_SCOPE)
    else()
        
        if(${library_type} MATCHES "static")
            set(CONFIG_PALTFORM_EXTENSION ".a")
        else()
            set(CONFIG_PALTFORM_EXTENSION ".so")
        endif()

        set(${library_debug} "lib${orgin}${CONFIG_PALTFORM_EXTENSION}" PARENT_SCOPE)
        set(${library_release} "lib${orgin}${CONFIG_PALTFORM_EXTENSION}" PARENT_SCOPE)
    endif()

endfunction()

function(library_regular_named_query_for_static library_debug library_release orgin)
    library_regular_named_query(_debuglib _releaselib "static" ${orgin})
    set(${library_debug} "lib${_debuglib}" PARENT_SCOPE)
    set(${library_release} "lib${_releaselib}" PARENT_SCOPE)
endfunction()

macro(create_named_static_import_target static_target orgin_name)
    library_regular_named_query_for_static(_debuglib _releaselib "static" ${orgin_name})
    add_library(${static_target} STATIC IMPORTED)
    set_target_properties(${static_target} PROPERTIES
      IMPORTED_LOCATION_DEBUG "${_debuglib}"
      IMPORTED_LOCATION_RELEASE "${_releaselib}"
      IMPORTED_LOCATION_MINSIZEREL "${_releaselib}"
      IMPORTED_LOCATION_RELWITHDEBINFO "${_releaselib}")

    get_target_property(LIBRARY_TARGE_TYPE ${static_target} TYPE)
    message(STATUS "**> _debuglib=${_debuglib} _releaselib=${_releaselib} LIBRARY_TARGE_TYPE=${LIBRARY_TARGE_TYPE}")
endmacro()

macro(create_named_shared_import_target static_target orgin_name)
    library_regular_named_query(_debuglib _releaselib "shared" ${orgin_name})
    add_library(${static_target} SHARED IMPORTED)
    set_target_properties(${static_target} PROPERTIES
      IMPORTED_LOCATION_DEBUG "${_debuglib}"
      IMPORTED_LOCATION_RELEASE "${_releaselib}"
      IMPORTED_LOCATION_MINSIZEREL "${_releaselib}"
      IMPORTED_LOCATION_RELWITHDEBINFO "${_releaselib}")
endmacro()
