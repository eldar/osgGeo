SET(LIB_NAME osgGeo)

CONFIGURE_FILE(
    ${CMAKE_CURRENT_SOURCE_DIR}/Config.cmake
    ${CMAKE_CURRENT_SOURCE_DIR}/Config)

ADD_LIBRARY( ${LIB_NAME} SHARED
    ${LIB_PUBLIC_HEADERS}
    Palette.cpp
    ShaderUtility.cpp
    PolyLine.cpp
    Horizon3DBase.cpp
    Horizon3D.cpp
    Horizon3D2.cpp
    LayeredTexture.cpp
    TexturePlane.cpp )

target_link_libraries(
    ${LIB_NAME}
    ${OSGGEO_OSG_LIBRARY}
    ${OSGGEO_OPENTHREADS_LIBRARY}
    ${OSGGEO_OSGUTIL_LIBRARY}
    ${OSGGEO_OSGDB_LIBRARY} )

INSTALL(FILES
    Export
    Common
    Config
    Horizon3DBase
    Horizon3D
    Horizon3D2
    LayeredTexture
    PolyLine
    TexturePlane
    Vec2i
    DESTINATION include/${LIB_NAME} )

INSTALL(TARGETS
    ${LIB_NAME}
    RUNTIME DESTINATION bin${OSGGEO_LIB_POSTFIX}
    LIBRARY DESTINATION lib${OSGGEO_LIB_POSTFIX}
    ARCHIVE DESTINATION lib${OSGGEO_LIB_POSTFIX} )
