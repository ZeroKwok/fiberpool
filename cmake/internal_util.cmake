function(install_pdb target_name)
	
    get_target_property(target_pdb_name_debug       ${target_name} COMPILE_PDB_NAME_DEBUG)
    get_target_property(target_pdb_name_release     ${target_name} COMPILE_PDB_NAME_RELEASE)
    get_target_property(target_pdb_output_directory ${target_name} PDB_OUTPUT_DIRECTORY)
	
	# 需要设置 set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
	# message("target_pdb_name_debug=${target_pdb_name_debug}")
	# message("target_pdb_output_directory=${target_pdb_output_directory}")

    # 在vs2019中, 文件输出目录为CMAKE_ARCHIVE_OUTPUT_DIRECTORY,
    # 在cmd中则是CMAKE_ARCHIVE_OUTPUT_DIRECTORY/CMAKE_INSTALL_CONFIG_NAME
    install(FILES
        "${target_pdb_output_directory}/\${CMAKE_INSTALL_CONFIG_NAME}/$<$<CONFIG:Debug>:${target_pdb_name_debug}>$<$<NOT:$<CONFIG:Debug>>:${target_pdb_name_release}>.pdb"
        DESTINATION lib
        OPTIONAL)

    install(FILES
        "${target_pdb_output_directory}/$<$<CONFIG:Debug>:${target_pdb_name_debug}>$<$<NOT:$<CONFIG:Debug>>:${target_pdb_name_release}>.pdb"
        DESTINATION lib
        OPTIONAL)
endfunction()