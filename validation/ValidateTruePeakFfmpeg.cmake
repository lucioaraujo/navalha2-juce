if(NOT DEFINED NAVALHA_TRUE_PEAK_TOOL
   OR NOT DEFINED NAVALHA_FFMPEG
   OR NOT DEFINED NAVALHA_TRUE_PEAK_OUTPUT_DIR)
    message(FATAL_ERROR "True-peak external validation arguments are missing")
endif()

file(MAKE_DIRECTORY "${NAVALHA_TRUE_PEAK_OUTPUT_DIR}")
execute_process(
    COMMAND "${NAVALHA_TRUE_PEAK_TOOL}" "${NAVALHA_TRUE_PEAK_OUTPUT_DIR}"
    RESULT_VARIABLE renderResult
    OUTPUT_VARIABLE renderOutput
    ERROR_VARIABLE renderError)
if(NOT renderResult EQUAL 0)
    message(FATAL_ERROR
        "True-peak fixture rendering failed:\n${renderOutput}${renderError}")
endif()

file(GLOB fixtureFiles
    "${NAVALHA_TRUE_PEAK_OUTPUT_DIR}/ebu3341_case*_f32.wav")
list(SORT fixtureFiles)
list(LENGTH fixtureFiles fixtureCount)
if(NOT fixtureCount EQUAL 18)
    message(FATAL_ERROR
        "Expected 18 true-peak WAVs, found ${fixtureCount}")
endif()

foreach(fixtureFile IN LISTS fixtureFiles)
    get_filename_component(fixtureName "${fixtureFile}" NAME)
    execute_process(
        COMMAND "${NAVALHA_FFMPEG}"
                -hide_banner -nostats -i "${fixtureFile}"
                -filter_complex ebur128=peak=true -f null -
        RESULT_VARIABLE ffmpegResult
        OUTPUT_VARIABLE ffmpegOutput
        ERROR_VARIABLE ffmpegLog)
    if(NOT ffmpegResult EQUAL 0)
        message(FATAL_ERROR
            "FFmpeg failed for ${fixtureName}:\n${ffmpegOutput}${ffmpegLog}")
    endif()
    string(REGEX MATCH
        "Peak:[ \t]*[-+0-9.]+ dBFS" peakMatch "${ffmpegLog}")
    if(NOT peakMatch)
        message(FATAL_ERROR
            "FFmpeg did not report true peak for ${fixtureName}")
    endif()
    string(REGEX REPLACE
        "Peak:[ \t]*([-+0-9.]+) dBFS" "\\1" peakValue "${peakMatch}")

    if(fixtureName MATCHES "_limited_f32\\.wav$")
        if(fixtureName MATCHES "case1[5-8]_")
            set(minimum -6.4)
            set(maximum -5.8)
        else()
            set(minimum -1.6)
            set(maximum -0.9)
        endif()
    elseif(fixtureName MATCHES "case1[5-8]_")
        set(minimum -6.4)
        set(maximum -5.8)
    elseif(fixtureName MATCHES "case19_")
        set(minimum 2.6)
        set(maximum 3.2)
    else()
        set(minimum -0.4)
        set(maximum 0.2)
    endif()

    if(peakValue LESS minimum OR peakValue GREATER maximum)
        message(FATAL_ERROR
            "${fixtureName}: ${peakValue} dBTP is outside "
            "[${minimum}, ${maximum}]")
    endif()
    message(STATUS "${fixtureName}: ${peakValue} dBTP")
endforeach()
