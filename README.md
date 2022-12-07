> Please refer to the HTML documentation in doc/DocBook/html/single.html for
instructions.


**Note**; to build it on modern Linux you'll need some older version of `gcc` to generate the `.tu` file, I used the `gcc-5` binary, see `site_scons/site_tools/tu.py`, anything older that supports `-fdump-translation-unit` should do. The rest of the program can be built with normal `gcc` like this:

    scons parts=debugger
    scons parts=interceptor without_lavc=true # swy: skip the ffmpeg stuff
    scons parts=docs

Keep in mind that `scons parts=tests` seems to have circular dependencies, but we don't care about that one.

To debug the SCons build `scons --debug=stacktrace` is useful.
