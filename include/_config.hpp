#ifndef fiber_pool_config_h__
#define fiber_pool_config_h__

/*!
 * \file   _config.hpp
 *
 * \author zero kwok
 * \date   2020-10
 *
 */

#if defined(FIBERPOOL_BUILD_SHARED_LIB)                 ///< build shared lib
#   define FIBER_POOL_DECL __declspec(dllexport)
#elif defined(FIBERPOOL_USING_SHARED_LIB)               ///< using shared lib
#   define FIBER_POOL_DECL __declspec(dllimport)
#else
#   define FIBER_POOL_DECL                              ///< using or build static lib
#endif

#if defined(WIN32) || defined(_WIN32)
#   ifndef WIN32_LEAN_AND_MEAN
#      define  WIN32_LEAN_AND_MEAN 
#   endif
#   ifndef  NOMINMAX
#      define  NOMINMAX
#   endif
#endif

#endif // fiber_pool_config_h__
