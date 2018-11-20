import os

from conans import ConanFile, tools


class CxxmetricsConan(ConanFile):
    name = "cxxmetrics"
    version = "0.0.4"
    license = "Apache 2.0"
    settings = "os"
    url = "https://github.com/kmaragon/cxxmetrics"
    description = "A smallish header-only C++14 library inspired by dropwizard metrics (codahale)"
    requires = "ctti/0.0.1@manu343726/testing"
    options = { "prometheus": [True, False] }
    default_options = "prometheus=True"
    exports_sources = "cxxmetrics*"
    no_copy_source = True
    # No settings/options are necessary, this is header only

    def package(self):
        self.copy("*.hpp", src="cxxmetrics", dst="include/cxxmetrics")
        if self.options.prometheus:
            self.copy("*.hpp", src="cxxmetrics_prometheus", dst="include/cxxmetrics_prometheus")

    def package_info(self):
        self.cpp_info.includedirs = ['include']
        if self.settings.os == 'Linux':
            self.cpp_info.libs = ["atomic"]

