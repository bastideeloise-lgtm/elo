
cmake_minimum_required(VERSION 3.20)

project(Projet_Domotique C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(Serveur
        serveur/main.c
        serveur/serveur.c
)

target_link_libraries(Serveur ws2_32)