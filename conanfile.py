from conan import ConanFile
from conan.tools.gnu import AutoToolsToolchain, Autotools
from conan.tools.files import get, copy, rmdir
from conan.tools.layout import basic_layout
import os


class PgnxDepsConan(ConanFile):
    name = "pgnx-deps"
    version = "1.0"
    description = "Dependencies for pgnx native addon"
    settings = "os", "compiler", "build_type", "arch"
    
    def requirements(self):
        # Core dependencies
        self.requires("openssl/3.2.0")
        self.requires("zlib/1.3.1")
        # Note: libpq and libpqxx need to be built manually as they're not in ConanCenter
        # We'll handle them in the build script
    
    def configure(self):
        # Force static linking for all dependencies
        self.options["openssl"].shared = False
        self.options["zlib"].shared = False
    
    def layout(self):
        basic_layout(self)
    
    def generate(self):
        # Generate environment variables for node-gyp
        deps_info = self.dependencies
        
        include_dirs = []
        lib_dirs = []
        libs = []
        
        for dep in deps_info.host_requires():
            cpp_info = dep.cpp_info.aggregated_components()
            include_dirs.extend(cpp_info.includedirs)
            lib_dirs.extend(cpp_info.libdirs)
            libs.extend(cpp_info.libs)
        
        # Write paths to a file that can be sourced
        with open(os.path.join(self.build_folder, "conan_paths.sh"), "w") as f:
            f.write(f"export CONAN_INCLUDE_DIRS=\"{':'.join(include_dirs)}\"\n")
            f.write(f"export CONAN_LIB_DIRS=\"{':'.join(lib_dirs)}\"\n")
            f.write(f"export CONAN_LIBS=\"{' '.join(['-l' + lib for lib in libs])}\"\n")
    
    def package_info(self):
        self.cpp_info.libs = []
