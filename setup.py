import setuptools
from distutils.core import setup

with open('README') as f:
    readme = f.read()

setup(
    name='pluto',
    version='1.1',
    packages=['pluto'],
    include_package_data=True,
    description='Interactive IPython kernel for Jupyter with a built in Vulkan graphics engine',
    long_description=readme,
    author='Nathan Morrical',
    author_email='natemorrical@gmail.com',
    url='https://github.com/n8vm/Pluto',
    install_requires=[
        'jupyter_client', 'IPython', 'ipykernel', 'numpy'
    ],
    classifiers=[
        'Intended Audience :: Developers',
        'License :: OSI Approved :: MIT License',
        'Programming Language :: Python :: 3',
    ],
)