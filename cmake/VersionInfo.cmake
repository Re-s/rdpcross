# ╔═══════════════════════════════════════════════════════════════════════════╗
# ║  RDPCross 构建版本信息                                                   ║
# ║                                                                          ║
#  规则（参考 vrndis 项目）：                                                ║
#    v<base>-<距离最近 tag 的提交数>-g<7位短hash>                              ║
#    例：v1.1.0-6-g856cbb0                                                   ║
#                                                                          ║
#  base 来自 src/constants.h 的 kAppVersion（人工发布时递增主版本号）。         ║
#  tag 不存在时用本地全部提交数，hash 取 7 位短 hash。                          ║
# ╚═══════════════════════════════════════════════════════════════════════════╝
function(rdpcross_git_version)
    set(BASE_VERSION "2.1.0")  # constants.h 缺失时的兜底，与 kAppVersion 保持同步

    # 1) 从 constants.h 取 base version（例如 2.0.0，不存在的兜底）
    # 注意：CMake 正则不支持 [[:space:]] POSIX 字符类，只能用显式字符集，
    # 以前的 [[:space:]] 写法静默失效，base 永远是下面硬编码的值。
    if(EXISTS ${CMAKE_SOURCE_DIR}/src/constants.h)
        file(READ ${CMAKE_SOURCE_DIR}/src/constants.h CONSTANTS_SRC)
        string(REGEX MATCH "kAppVersion[ \t\r\n]*=[ \t\r\n]*\"([0-9.]+)\"" _m "${CONSTANTS_SRC}")
        if(_m)
            set(BASE_VERSION "${CMAKE_MATCH_1}")
        endif()
    endif()

    # 2) 7 位短 hash（git rev-parse --short=7 HEAD）
    execute_process(
        COMMAND git rev-parse --short=7 HEAD
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        OUTPUT_VARIABLE GIT_HASH
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
        RESULT_VARIABLE rc
    )
    if(NOT rc EQUAL 0 OR GIT_HASH STREQUAL "")
        set(GIT_HASH "unknown")
    endif()

    # 3) 最近 tag 描述: v2.0.0-12-g1234567
    execute_process(
        COMMAND git describe --tags --long --always --dirty=-dirty
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        OUTPUT_VARIABLE GIT_DESCRIBE
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
        RESULT_VARIABLE rc
    )
    if(NOT rc EQUAL 0 OR GIT_DESCRIBE STREQUAL "")
        set(GIT_DESCRIBE "")
    endif()

    if(GIT_DESCRIBE MATCHES "^v?[0-9.]+-[0-9]+-g[0-9a-f]+$")
        # 解析: v2.0.0-12-g1234567
        string(REGEX MATCH "^v?([0-9.]+)-([0-9]+)-g([0-9a-f]+)" _m "${GIT_DESCRIBE}")
        set(BASE_VERSION "${CMAKE_MATCH_1}")
        set(GIT_COMMITS "${CMAKE_MATCH_2}")
        set(GIT_HASH "${CMAKE_MATCH_3}")
    else()
        # 无 tag：用本地提交总数（绝对编号，保证自动递增）
        execute_process(
            COMMAND git rev-list --count HEAD
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
            OUTPUT_VARIABLE GIT_COMMITS
            OUTPUT_STRIP_TRAILING_WHITESPACE
            ERROR_QUIET
            RESULT_VARIABLE rc
        )
        if(NOT rc EQUAL 0 OR GIT_COMMITS STREQUAL "")
            set(GIT_COMMITS 0)
        endif()
        set(GIT_DESCRIBE "v${BASE_VERSION}-${GIT_COMMITS}-g${GIT_HASH}")
    endif()

    set(GIT_VERSION_STR "${BASE_VERSION}-${GIT_COMMITS}-g${GIT_HASH}")
    set(GIT_VERSION_FULL "v${GIT_VERSION_STR}")

    # 4) 落盘版本字符，CI 发布 / 文档 / AppImage 文件名用
    file(WRITE ${CMAKE_BINARY_DIR}/VERSION_FULL.txt "${GIT_VERSION_STR}")

    # 推给 CMakeLists.txt 用
    set(GIT_VERSION_STR "${GIT_VERSION_STR}" PARENT_SCOPE)
    set(GIT_VERSION_FULL "v${GIT_VERSION_STR}" PARENT_SCOPE)
    set(GIT_VERSION_BASE "${BASE_VERSION}" PARENT_SCOPE)
    set(GIT_VERSION_COMMITS "${GIT_COMMITS}" PARENT_SCOPE)
    set(GIT_VERSION_HASH "${GIT_HASH}" PARENT_SCOPE)
endfunction()
