import os
import sys
import shutil

from setuptools import setup
from setuptools.command.install import install

with open('README.md') as f:
    readme = f.read()

# from https://stackoverflow.com/questions/45150304/how-to-force-a-python-wheel-to-be-platform-specific-when-building-it # noqa
try:
    from wheel.bdist_wheel import bdist_wheel as _bdist_wheel

    class bdist_wheel(_bdist_wheel):
        def finalize_options(self):
            _bdist_wheel.finalize_options(self)
            self.root_is_pure = False

except ImportError:
    bdist_wheel = None

executable_name = "pluto.exe" if sys.platform.startswith("win") else "pluto"
lib_name = "PlutoLib.dll" if sys.platform.startswith("win") else ""
resource_dir_name = "Resources"

class PostInstallCommand(install):
    """Post-installation for installation mode."""
    def run(self):
        install.run(self)

        # Get the directory containing PlutoEngine
        source_dir = os.path.dirname(os.path.abspath(__file__))

        # We're going to install PlutoEngine into the scripts directory
        # but first make sure the scripts directory exists
        if not os.path.isdir(self.install_scripts):
            os.makedirs(self.install_scripts)

        pluto_executable_source = os.path.join(source_dir, "Pluto", executable_name)
        pluto_lib_source = os.path.join(source_dir, "Pluto", lib_name)
        pluto_resources_source = os.path.join(source_dir, "Pluto", resource_dir_name)

        pluto_executable_target = os.path.join(self.install_scripts, executable_name)
        pluto_lib_target = os.path.join(self.install_scripts, lib_name)
        pluto_resources_target = os.path.join(self.install_scripts, resource_dir_name)

        print("Copying ", pluto_executable_source, " to ", pluto_executable_target)
        print("Copying ", pluto_lib_source, " to ", pluto_lib_target)
        print("Copying ", pluto_resources_source, " to ", pluto_resources_target)

        if os.path.isfile(pluto_executable_target):
            os.remove(pluto_executable_target)
        
        if os.path.isfile(pluto_lib_target):
            os.remove(pluto_lib_target)

        if os.path.isdir(os.path.join(pluto_resources_target, resource_dir_name)):
            shutil.rmtree(os.path.join(pluto_resources_target, resource_dir_name) )

        self.move_file(pluto_executable_source, pluto_executable_target)
        self.move_file(pluto_lib_source, pluto_lib_target)
        self.copy_tree(pluto_resources_source, pluto_resources_target)
        shutil.rmtree(pluto_resources_source)

setup(
    name='Pluto',
    version='1.1.1',
    packages=['Pluto'],
    python_requires='>3.5.2',
    include_package_data=True,
    description='Interactive IPython kernel for Jupyter with a built in Vulkan graphics engine',
    long_description=readme,
    author='Nathan Morrical',
    author_email='natemorrical@gmail.com',
    url='https://github.com/n8vm/Pluto',
    install_requires=[],
    classifiers=[
        'Intended Audience :: Developers',
        'License :: OSI Approved :: MIT License',
        'Programming Language :: Python :: 3.7'
    ],
    cmdclass={'install': PostInstallCommand, 'bdist_wheel': bdist_wheel},
)