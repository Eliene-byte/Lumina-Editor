# Packaging.cmake
#
# Monta a pasta portavel (sem instalador) e o .zip distribuivel.
# Chamado pelo CMakeLists raiz via lumina_configure_packaging().

set(LUMINA_BUNDLE_NAME "Lumina")

function(lumina_configure_packaging)
    if(NOT LUMINA_BUILD_APP)
        return()
    endif()

    set(_exe_target lumina)
    if(NOT TARGET ${_exe_target})
        return()
    endif()

    # --- Titulo das janelas / metadados no Windows -----------------------------
    if(WIN32)
        set_target_properties(${_exe_target} PROPERTIES
            OUTPUT_NAME "lumina"
            VS_GLOBAL_AppTitle "${LUMINA_BUNDLE_NAME}"
            VS_GLOBAL_ProductName "${LUMINA_BUNDLE_NAME}"
            VS_GLOBAL_CompanyName "Lumina"
            VS_GLOBAL_FileVersion "${PROJECT_VERSION}"
            VS_GLOBAL_ProductVersion "${PROJECT_VERSION}"
            VS_GLOBAL_LegalCopyright "LGPL-3.0-or-later"
        )
    endif()

    # --- Instalacao ------------------------------------------------------------
    install(TARGETS ${_exe_target}
        RUNTIME DESTINATION .
        BUNDLE  DESTINATION .
    )

    if(APPLE)
        install(CODE "
            file(MAKE_DIRECTORY \"\$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/Lumina.app/Contents/MacOS\")
        ")
    endif()

    # --- Copia automatica das DLLs do Qt e do FFmpeg --------------------------
    if(MSVC)
        # Executado no post-build: evita depender do install(CODE) e da ordem.
        add_custom_command(TARGET ${_exe_target} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E env
                    "PATH=$<TARGET_FILE_DIR:Qt6::Core>;$ENV{PATH}"
                    ${QT_DEPLOY_TOOL}
                    --no-translations --no-system-d3d-compiler --no-opengl-sw
                    --no-quick-import --compiler-runtime
                    "$<TARGET_FILE:${_exe_target}>"
            COMMENT "Deployando Qt/FFmpeg na pasta portatil"
            VERBATIM
        )
    endif()
endfunction()
