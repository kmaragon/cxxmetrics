import os

from conans import ConanFile, CMake, tools

class CxxmetricsTestConan(ConanFile):
    settings = "os", "compiler", "build_type", "arch"
    generators = "cmake"
    exports = "CMakeLists.txt", "../test*"
    build_requires = "catch2/2.3.0@bincrafters/stable"

    def source(self):
        tools.replace_in_file("CMakeLists.txt", "project(\"cxxmetrics_test\" CXX)", '''project("cxxmetrics_test" CXX)
include(${CMAKE_BINARY_DIR}/conanbuildinfo.cmake)
conan_basic_setup(TARGETS NO_OUTPUT_DIRS)''')

    def build(self):
        cmake = CMake(self)
        # Current dir is "test_package/build/<build_id>" and CMakeLists.txt is
        # in "test_package"
        cmake.configure()
        cmake.build()

    def imports(self):
        self.copy("*.dll", dst="bin", src="bin")
        self.copy("*.dylib*", dst="bin", src="lib")
        self.copy('*.so*', dst='bin', src='lib')

    def test(self):
        if not tools.cross_building(self.settings):
            self.run(".%scxxmetrics_test" % os.sep)
