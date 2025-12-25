set(SRCS
    main.cpp
    Window.cpp
    Window.h
    Scene/Scene.cpp
    Scene/Scene.h
    Scene/Camera.cpp
    Scene/Camera.h

    Shaders/diffuse.fs
    Shaders/diffuse.vs
    Shaders/model.fs
    Shaders/model.vs
    Textures/voronoi.png

    resources.qrc
)

find_package(Qt5 COMPONENTS Widgets REQUIRED)

add_executable(demo-app ${SRCS})

target_link_libraries(demo-app
    PRIVATE
        Qt5::Widgets
        FGL::Base
        thirdparty::tinygltf
)