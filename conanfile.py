from conan import ConanFile, tools
from conan.tools.layout import basic_layout
from conan.tools.files import copy
from conan.errors import ConanInvalidConfiguration
from conan.tools.build import check_min_cppstd
import os

class CxxmetricsConan(ConanFile):
    name = "cxxmetrics"
    description = (
        "A smallish header-only C++14 library inspired by dropwizard metrics (codahale)"
    )
    license = "Apache 2.0"
    url = "https://github.com/kmaragon/cxxmetrics"
    settings = ("compiler", "os")
    options = { "with_prometheus": [True, False] }
    default_options = { "with_prometheus": True }
    package_type = "header-library"
    exports_sources = "CMakeLists.txt", "cxxmetrics*"
    no_copy_source = True

    @property
    def _min_cppstd(self):
        return "14"

    def layout(self):
        basic_layout(self, src_folder=".")

    def validate(self):
        if self.settings.compiler.get_safe("cppstd"):
            check_min_cppstd(self, self._min_cppstd)

    def requirements(self):
        pass

    def package(self):
        copy(self,
             "*.hpp",
             os.path.join(self.source_folder, "cxxmetrics"),
             os.path.join(self.package_folder, "include/cxxmetrics"))
        if self.options.with_prometheus:
            copy(self,
                 "*.hpp",
                 os.path.join(self.source_folder, "cxxmetrics_prometheus"),
                 os.path.join(self.package_folder, "include/cxxmetrics_prometheus"))

    def package_info(self):
        self.cpp_info.set_property("cmake_file_name", "cxxmetrics")
        self.cpp_info.components["cxxmetrics"].names["cmake_find_package"] = "cxxmetrics"
        self.cpp_info.components["cxxmetrics"].includedirs = ['include']

        if self.settings.os == 'Linux':
            self.cpp_info.components["cxxmetrics"].system_libs = ["atomic"]

