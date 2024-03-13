# Find "ModuleTools" dependencies, which are other ModuleTools packages.
set(Qt6LinguistTools_FOUND FALSE)
set(__qt_LinguistTools_tool_deps "")
foreach(__qt_LinguistTools_target_dep ${__qt_LinguistTools_tool_deps})
    list(GET __qt_LinguistTools_target_dep 0 __qt_LinguistTools_pkg)
    list(GET __qt_LinguistTools_target_dep 1 __qt_LinguistTools_version)

    if (NOT ${__qt_LinguistTools_pkg}_FOUND)
        find_dependency(${__qt_LinguistTools_pkg} ${__qt_LinguistTools_version})
    endif()
endforeach()

set(Qt6LinguistTools_FOUND TRUE)
