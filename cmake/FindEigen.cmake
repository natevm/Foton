# Additional modules
include(FindPackageHandleStandardArgs)

if (WIN32)
	# Find include files
	find_path(
		EIGEN_INCLUDE_DIR
		NAMES Eigen/Eigen
		PATHS
		"../external/eigen"
        "./external/eigen"
		$ENV{GLM_ROOT_DIR}
		${GLM_ROOT_DIR}
		$ENV{GLM_ROOT_DIR}/include
		${GLM_ROOT_DIR}/include
		DOC "The directory where Eigen/Eigen resides")
else()
	# Find include files
	find_path(
		EIGEN_INCLUDE_DIR
		NAMES Eigen/Eigen
		PATHS
		"../external/eigen"
        "./external/eigen"
		/usr/include
		/usr/local/include
		/sw/include
		/opt/local/include
		${GLM_ROOT_DIR}/include
		DOC "The directory where Eigen/Eigen resides")
endif()

# Handle REQUIRD argument, define *_FOUND variable
find_package_handle_standard_args(EIGEN DEFAULT_MSG EIGEN_INCLUDE_DIR)

# Hide some variables
mark_as_advanced(EIGEN_INCLUDE_DIR)