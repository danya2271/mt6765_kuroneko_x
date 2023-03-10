/*************************************************************************/ /*!
@File
@Title          PVR Debug Declarations
@Copyright      Copyright (c) Imagination Technologies Ltd. All Rights Reserved
@Description    Provides debug functionality
@License        Dual MIT/GPLv2

The contents of this file are subject to the MIT license as set out below.

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

Alternatively, the contents of this file may be used under the terms of
the GNU General Public License Version 2 ("GPL") in which case the provisions
of GPL are applicable instead of those above.

If you wish to allow use of your version of this file only under the terms of
GPL, and not to allow others to use your version of this file under the terms
of the MIT license, indicate your decision by deleting the provisions above
and replace them with the notice and other provisions required by GPL as set
out in the file called "GPL-COPYING" included in this distribution. If you do
not delete the provisions above, a recipient may use your version of this file
under the terms of either the MIT license or GPL.

This License is also included in this distribution in the file called
"MIT-COPYING".

EXCEPT AS OTHERWISE STATED IN A NEGOTIATED AGREEMENT: (A) THE SOFTWARE IS
PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING
BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
PURPOSE AND NONINFRINGEMENT; AND (B) IN NO EVENT SHALL THE AUTHORS OR
COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/ /**************************************************************************/

#ifndef PVR_DEBUG_H
#define PVR_DEBUG_H

#include "img_defs.h"
#include "img_types.h"
#include "pvrsrv_error.h"

/*! @cond Doxygen_Suppress */
#if defined(_MSC_VER)
#	define MSC_SUPPRESS_4127 __pragma(warning(suppress:4127))
#else
#	define MSC_SUPPRESS_4127
#endif
/*! @endcond */

#if defined(__cplusplus)
extern "C" {
#endif

#define PVR_MAX_DEBUG_MESSAGE_LEN	(512)   /*!< Max length of a Debug Message */

/* These are privately used by pvr_debug, use the PVR_DBG_ defines instead */
#define DBGPRIV_FATAL     0x001UL  /*!< Debug-Fatal. Privately used by pvr_debug. */
#define DBGPRIV_ERROR     0x002UL  /*!< Debug-Error. Privately used by pvr_debug. */
#define DBGPRIV_WARNING   0x004UL  /*!< Debug-Warning. Privately used by pvr_debug. */
#define DBGPRIV_MESSAGE   0x008UL  /*!< Debug-Message. Privately used by pvr_debug. */
#define DBGPRIV_VERBOSE   0x010UL  /*!< Debug-Verbose. Privately used by pvr_debug. */
#define DBGPRIV_CALLTRACE 0x020UL  /*!< Debug-CallTrace. Privately used by pvr_debug. */
#define DBGPRIV_ALLOC     0x040UL  /*!< Debug-Alloc. Privately used by pvr_debug. */
#define DBGPRIV_BUFFERED  0x080UL  /*!< Debug-Buffered. Privately used by pvr_debug. */
#define DBGPRIV_DEBUG     0x100UL  /*!< Debug-AdHoc-Debug. Never submitted. Privately used by pvr_debug. */
#define DBGPRIV_LAST      0x200UL  /*!< Always set to highest mask value. Privately used by pvr_debug. */

#if !defined(PVRSRV_NEED_PVR_ASSERT) && defined(DEBUG)
#endif

#if defined(PVRSRV_NEED_PVR_ASSERT) && !defined(PVRSRV_NEED_PVR_DPF)
#endif

#if !defined(PVRSRV_NEED_PVR_TRACE) && (defined(DEBUG) || defined(TIMING))
#endif

#if !defined(DOXYGEN)
/*************************************************************************/ /*
PVRSRVGetErrorString
Returns a string describing the provided PVRSRV_ERROR code
NB No doxygen comments provided as this function does not require porting
   for other operating systems
*/ /**************************************************************************/
	const IMG_CHAR *PVRSRVGetErrorString(PVRSRV_ERROR eError);
#	define PVRSRVGETERRORSTRING PVRSRVGetErrorString
#endif

/* PVR_ASSERT() and PVR_DBG_BREAK handling */

#if defined(PVRSRV_NEED_PVR_ASSERT) || defined(DOXYGEN)

/* Unfortunately the klocworks static analysis checker doesn't understand our
 * ASSERT macros. Thus it reports lots of false positive. Defining our Assert
 * macros in a special way when the code is analysed by klocworks avoids
 * them. */
#if defined(__KLOCWORK__)
  #define PVR_ASSERT(x) do { if (!(x)) abort(); } while (0)
#else /* ! __KLOCWORKS__ */

#if defined(_WIN32)
#define PVR_ASSERT(expr)
#else

#if defined(LINUX) && defined(__KERNEL__)
#include <linux/kernel.h>
#include <linux/bug.h>

#define PVR_ASSERT(EXPR) do
#else /* defined(LINUX) && defined(__KERNEL__) */

/*************************************************************************/ /*!
@Function       PVRSRVDebugAssertFail
@Description    Indicate to the user that a debug assertion has failed and
                prevent the program from continuing.
                Invoked from the macro PVR_ASSERT().
@Input          pszFile       The name of the source file where the assertion failed
@Input          ui32Line      The line number of the failed assertion
@Input          pszAssertion  String describing the assertion
@Return         NEVER!
*/ /**************************************************************************/
IMG_EXPORT void IMG_CALLCONV __noreturn
PVRSRVDebugAssertFail(const IMG_CHAR *pszFile,
                      IMG_UINT32 ui32Line,
                      const IMG_CHAR *pszAssertion);

#define PVR_ASSERT(EXPR)
#endif /* defined(LINUX) && defined(__KERNEL__) */
#endif /* defined(_WIN32) */
#endif /* defined(__KLOCWORK__) */

#if defined(__KLOCWORK__)
	#define PVR_DBG_BREAK do { abort(); } while (0)
#else
	#if defined(WIN32)
		#define PVR_DBG_BREAK __debugbreak()   /*!< Implementation of PVR_DBG_BREAK for (non-WinCE) Win32 */
	#else
		#if defined(PVR_DBG_BREAK_ASSERT_FAIL)
		/*!< Implementation of PVR_DBG_BREAK that maps onto PVRSRVDebugAssertFail */
			#if defined(_WIN32)
				#define PVR_DBG_BREAK	DBG_BREAK
			#else
				#if defined(LINUX) && defined(__KERNEL__)
					#define PVR_DBG_BREAK BUG()
				#else
					#define PVR_DBG_BREAK	PVRSRVDebugAssertFail(__FILE__, __LINE__, "PVR_DBG_BREAK")
				#endif
			#endif
		#else
			/*!< Null Implementation of PVR_DBG_BREAK (does nothing) */
			#define PVR_DBG_BREAK
		#endif
	#endif
#endif


#else  /* defined(PVRSRV_NEED_PVR_ASSERT) */
    /* Unfortunately the klocworks static analysis checker doesn't understand our
     * ASSERT macros. Thus it reports lots of false positive. Defining our Assert
     * macros in a special way when the code is analysed by klocworks avoids
     * them. */
    #if defined(__KLOCWORK__)
        #define PVR_ASSERT(EXPR) 
    #else
        #define PVR_ASSERT(EXPR) 
    #endif

    #define PVR_DBG_BREAK    /*!< Null Implementation of PVR_DBG_BREAK (does nothing) */

#endif /* defined(PVRSRV_NEED_PVR_ASSERT) */


/* PVR_DPF() handling */

#if defined(PVRSRV_NEED_PVR_DPF) || defined(DOXYGEN)

	/* New logging mechanism */
	#define PVR_DBG_FATAL     DBGPRIV_FATAL     /*!< Debug level passed to PVRSRVDebugPrintf() for fatal errors. */
	#define PVR_DBG_ERROR     DBGPRIV_ERROR     /*!< Debug level passed to PVRSRVDebugPrintf() for non-fatal errors. */
	#define PVR_DBG_WARNING   DBGPRIV_WARNING   /*!< Debug level passed to PVRSRVDebugPrintf() for warnings. */
	#define PVR_DBG_MESSAGE   DBGPRIV_MESSAGE   /*!< Debug level passed to PVRSRVDebugPrintf() for information only. */
	#define PVR_DBG_VERBOSE   DBGPRIV_VERBOSE   /*!< Debug level passed to PVRSRVDebugPrintf() for very low-priority debug. */
	#define PVR_DBG_CALLTRACE DBGPRIV_CALLTRACE /*!< Debug level passed to PVRSRVDebugPrintf() for function tracing purposes. */
	#define PVR_DBG_ALLOC     DBGPRIV_ALLOC     /*!< Debug level passed to PVRSRVDebugPrintf() for tracking some of drivers memory operations. */
	#define PVR_DBG_BUFFERED  DBGPRIV_BUFFERED  /*!< Debug level passed to PVRSRVDebugPrintf() when debug should be written to the debug circular buffer. */
	#define PVR_DBG_DEBUG     DBGPRIV_DEBUG     /*!< Debug level passed to PVRSRVDebugPrintf() for debug messages. */

	/* These levels are always on with PVRSRV_NEED_PVR_DPF */
	/*! @cond Doxygen_Suppress */
	#define __PVR_DPF_0x001UL(...)
	#define __PVR_DPF_0x002UL(...)
	#define __PVR_DPF_0x080UL(...)

	/*
	  The AdHoc-Debug level is only supported when enabled in the local
	  build environment and may need to be used in both debug and release
	  builds. An error is generated in the formal build if it is checked in.
	*/
#if defined(PVR_DPF_ADHOC_DEBUG_ON)
	#define __PVR_DPF_0x100UL(...)
#else
    /* Use an undefined token here to stop compilation dead in the offending module */
	#define __PVR_DPF_0x100UL(...) 
#endif

	/* Some are compiled out completely in release builds */
#if defined(DEBUG) || defined(DOXYGEN)
	#define __PVR_DPF_0x004UL(...) 
	#define __PVR_DPF_0x008UL(...)
	#define __PVR_DPF_0x010UL(...)
	#define __PVR_DPF_0x020UL(...)
	#define __PVR_DPF_0x040UL(...)
#else
	#define __PVR_DPF_0x004UL(...)
	#define __PVR_DPF_0x008UL(...)
	#define __PVR_DPF_0x010UL(...)
	#define __PVR_DPF_0x020UL(...)
	#define __PVR_DPF_0x040UL(...)
	#define __PVR_DPF_0x200UL(...)
#endif

	/* Translate the different log levels to separate macros
	 * so they can each be compiled out.
	 */
#if defined(DEBUG)
	#define __PVR_DPF(lvl, ...)
#else
	#define __PVR_DPF(lvl, ...)
#endif
	/*! @endcond */

	/* Get rid of the double bracketing */
	#define PVR_DPF(x) __PVR_DPF x

	#define PVR_LOG_ERROR(_rc, _call) 

	#define PVR_LOG_IF_ERROR(_rc, _call)

	#define PVR_LOGR_IF_NOMEM(_expr, _call)

	#define PVR_LOGG_IF_NOMEM(_expr, _call, _err, _go)
	#define PVR_LOGR_IF_ERROR(_rc, _call)

	#define PVR_LOGRN_IF_ERROR(_rc, _call)

	#define PVR_LOGG_IF_ERROR(_rc, _call, _go) 
	#define PVR_LOG_IF_FALSE(_expr, _msg)

	#define PVR_LOGR_IF_FALSE(_expr, _msg, _rc)

	#define PVR_LOGG_IF_FALSE(_expr, _msg, _go)

/*************************************************************************/ /*!
@Function       PVRSRVDebugPrintf
@Description    Output a debug message to the user, using an OS-specific
                method, to a log or console which can be read by developers
                Invoked from the macro PVR_DPF().
@Input          ui32DebugLevel   The debug level of the message. This can
                                 be used to restrict the output of debug
                                 messages based on their severity.
                                 If this is PVR_DBG_BUFFERED, the message
                                 should be written into a debug circular
                                 buffer instead of being output immediately
                                 (useful when performance would otherwise
                                 be adversely affected).
                                 The debug circular buffer shall only be
                                 output when PVRSRVDebugPrintfDumpCCB() is
                                 called.
@Input          pszFileName      The source file containing the code that is
                                 generating the message
@Input          ui32Line         The line number in the source file
@Input          pszFormat        The formatted message string
@Input          ...              Zero or more arguments for use by the
                                 formatted string
@Return         None
*/ /**************************************************************************/
IMG_EXPORT void IMG_CALLCONV PVRSRVDebugPrintf(IMG_UINT32 ui32DebugLevel,
                                               const IMG_CHAR *pszFileName,
                                               IMG_UINT32 ui32Line,
                                               const IMG_CHAR *pszFormat,
                                               ...) __printf(4, 5);

/*************************************************************************/ /*!
@Function       PVRSRVDebugPrintfDumpCCB
@Description    When PVRSRVDebugPrintf() is called with the ui32DebugLevel
                specified as DBGPRIV_BUFFERED, the debug shall be written to
                the debug circular buffer instead of being output immediately.
                (This could be used to obtain debug without incurring a
                performance hit by printing it at that moment).
                This function shall dump the contents of that debug circular
                buffer to be output in an OS-specific method to a log or
                console which can be read by developers.
@Return         None
*/ /**************************************************************************/
IMG_EXPORT void IMG_CALLCONV PVRSRVDebugPrintfDumpCCB(void);

#else  /* defined(PVRSRV_NEED_PVR_DPF) */

	#define PVR_DPF(X)  /*!< Null Implementation of PowerVR Debug Printf (does nothing) */

	#define PVR_LOG_ERROR(_rc, _call) (void)(_rc)
	#define PVR_LOG_IF_ERROR(_rc, _call) (void)(_rc)

	#define PVR_LOGR_IF_NOMEM(_expr, _call) do { if (unlikely(_expr == NULL)) { return PVRSRV_ERROR_OUT_OF_MEMORY; } MSC_SUPPRESS_4127 } while (0)
	#define PVR_LOGG_IF_NOMEM(_expr, _call, _err, _go) do { if (unlikely(_expr == NULL)) { _err = PVRSRV_ERROR_OUT_OF_MEMORY; goto _go; } MSC_SUPPRESS_4127	} while (0)
	#define PVR_LOGR_IF_ERROR(_rc, _call) do { if (unlikely(_rc != PVRSRV_OK)) { return (_rc); } MSC_SUPPRESS_4127 } while(0)
	#define PVR_LOGRN_IF_ERROR(_rc, _call) do { if (unlikely(_rc != PVRSRV_OK)) { return; } MSC_SUPPRESS_4127 } while(0)
	#define PVR_LOGG_IF_ERROR(_rc, _call, _go) do { if (unlikely(_rc != PVRSRV_OK)) { goto _go; } MSC_SUPPRESS_4127 } while(0)

	#define PVR_LOG_IF_FALSE(_expr, _msg) (void)(_expr)
	#define PVR_LOGR_IF_FALSE(_expr, _msg, _rc) do { if (unlikely(!(_expr))) { return (_rc); } MSC_SUPPRESS_4127 } while(0)
	#define PVR_LOGG_IF_FALSE(_expr, _msg, _go) do { if (unlikely(!(_expr))) { goto _go; } MSC_SUPPRESS_4127 } while(0)

	#undef PVR_DPF_FUNCTION_TRACE_ON

#endif /* defined(PVRSRV_NEED_PVR_DPF) */

#define PVR_RETURN_IF_ERROR(_rc) 
#if defined(DEBUG)
	#define PVR_LOG_WARN(_rc, _call) 
	#define PVR_LOG_WARN_IF_ERROR(_rc, _call) 
#else
	#define PVR_LOG_WARN(_rc, _call)
	#define PVR_LOG_WARN_IF_ERROR(_rc, _call)
#endif

/*! @cond Doxygen_Suppress */
#if defined(PVR_DPF_FUNCTION_TRACE_ON)

	#define PVR_DPF_ENTERED 
	#define PVR_DPF_ENTERED1(p1) 
	#define PVR_DPF_RETURN_RC(a) 
	#define PVR_DPF_RETURN_RC1(a,p1) 
	#define PVR_DPF_RETURN_VAL(a) 
	#define PVR_DPF_RETURN_OK 
	#define PVR_DPF_RETURN 
	#if !defined(DEBUG)
	#error PVR DPF Function trace enabled in release build, rectify
	#endif

#else /* defined(PVR_DPF_FUNCTION_TRACE_ON) */

	#define PVR_DPF_ENTERED
	#define PVR_DPF_ENTERED1(p1)
	#define PVR_DPF_RETURN_RC(a)     return (a)
	#define PVR_DPF_RETURN_RC1(a,p1) return (a)
	#define PVR_DPF_RETURN_VAL(a)    return (a)
	#define PVR_DPF_RETURN_OK        return PVRSRV_OK
	#define PVR_DPF_RETURN           return

#endif /* defined(PVR_DPF_FUNCTION_TRACE_ON) */
/*! @endcond */

#if defined(__KERNEL__) || defined(DOXYGEN) || defined(__QNXNTO__)
/*Use PVR_DPF() unless message is necessary in release build */
#ifdef PVR_DISABLE_LOGGING
#define PVR_LOG(X)
#else
#define PVR_LOG(X) PVRSRVReleasePrintf X
#endif

/*************************************************************************/ /*!
@Function       PVRSRVReleasePrintf
@Description    Output an important message, using an OS-specific method,
                to a log or console which can be read by developers in
                release builds.
                Invoked from the macro PVR_LOG().
@Input          pszFormat   The message format string
@Input          ...         Zero or more arguments for use by the format string
@Return         None
*/ /**************************************************************************/
void IMG_CALLCONV PVRSRVReleasePrintf(const IMG_CHAR *pszFormat, ...) __printf(1, 2);
#endif

/* PVR_TRACE() handling */

#if defined(PVRSRV_NEED_PVR_TRACE) || defined(DOXYGEN)

	#define PVR_TRACE(X)	PVRSRVTrace X    /*!< PowerVR Debug Trace Macro */
	/* Empty string implementation that is -O0 build friendly */
	#define PVR_TRACE_EMPTY_LINE()	PVR_TRACE(("%s", ""))

/*************************************************************************/ /*!
@Function       PVRTrace
@Description    Output a debug message to the user
                Invoked from the macro PVR_TRACE().
@Input          pszFormat   The message format string
@Input          ...         Zero or more arguments for use by the format string
*/ /**************************************************************************/
IMG_EXPORT void IMG_CALLCONV PVRSRVTrace(const IMG_CHAR* pszFormat, ... )
	__printf(1, 2);

#else /* defined(PVRSRV_NEED_PVR_TRACE) */
    /*! Null Implementation of PowerVR Debug Trace Macro (does nothing) */
	#define PVR_TRACE(X)

#endif /* defined(PVRSRV_NEED_PVR_TRACE) */


#if defined(PVRSRV_NEED_PVR_ASSERT)
#ifdef INLINE_IS_PRAGMA
#pragma inline(TRUNCATE_64BITS_TO_32BITS)
#endif
	INLINE static IMG_UINT32 TRUNCATE_64BITS_TO_32BITS(IMG_UINT64 uiInput)
	{
		 IMG_UINT32 uiTruncated;

		 uiTruncated = (IMG_UINT32)uiInput;
		 PVR_ASSERT(uiInput == uiTruncated);
		 return uiTruncated;
	}


#ifdef INLINE_IS_PRAGMA
#pragma inline(TRUNCATE_64BITS_TO_SIZE_T)
#endif
	INLINE static size_t TRUNCATE_64BITS_TO_SIZE_T(IMG_UINT64 uiInput)
	{
		 size_t uiTruncated;

		 uiTruncated = (size_t)uiInput;
		 PVR_ASSERT(uiInput == uiTruncated);
		 return uiTruncated;
	}


#ifdef INLINE_IS_PRAGMA
#pragma inline(TRUNCATE_SIZE_T_TO_32BITS)
#endif
	INLINE static IMG_UINT32 TRUNCATE_SIZE_T_TO_32BITS(size_t uiInput)
	{
		 IMG_UINT32 uiTruncated;

		 uiTruncated = (IMG_UINT32)uiInput;
		 PVR_ASSERT(uiInput == uiTruncated);
		 return uiTruncated;
	}


#else /* defined(PVRSRV_NEED_PVR_ASSERT) */
	#define TRUNCATE_64BITS_TO_32BITS(expr) ((IMG_UINT32)(expr))
	#define TRUNCATE_64BITS_TO_SIZE_T(expr) ((size_t)(expr))
	#define TRUNCATE_SIZE_T_TO_32BITS(expr) ((IMG_UINT32)(expr))
#endif /* defined(PVRSRV_NEED_PVR_ASSERT) */

/*! @cond Doxygen_Suppress */
/* Macros used to trace calls */
#if defined(DEBUG)
	#define PVR_DBG_FILELINE 
	#define PVR_DBG_FILELINE_PARAM 
	#define PVR_DBG_FILELINE_ARG 
	#define PVR_DBG_FILELINE_FMT
	#define PVR_DBG_FILELINE_UNREF() 
#else
	#define PVR_DBG_FILELINE
	#define PVR_DBG_FILELINE_PARAM
	#define PVR_DBG_FILELINE_ARG
	#define PVR_DBG_FILELINE_FMT
	#define PVR_DBG_FILELINE_UNREF()
#endif
/*! @endcond */

#if defined(__cplusplus)
}
#endif

/*!
    @def PVR_ASSERT
    @brief Aborts the program if assertion fails.

    The macro will be defined only when PVRSRV_NEED_PVR_ASSERT macro is
    enabled. It's ignored otherwise.

    @def PVR_DPF
    @brief PowerVR Debug Printf logging macro used throughout the driver.

    The macro allows to print logging messages to appropriate log. The
    destination log is based on the component (user space / kernel space) and
    operating system (Linux, Android, etc.).

    The macro also supports severity levels that allow to turn on/off messages
    based on their importance.

    This macro will print messages with severity level higher that error only
    if PVRSRV_NEED_PVR_DPF macro is defined.

    @def PVR_LOG_ERROR
    @brief Logs error.

    @def PVR_LOG_IF_ERROR
    @brief Logs error if not PVRSRV_OK.

    @def PVR_LOGR_IF_NOMEM
    @brief Logs error if expression is NULL and returns PVRSRV_ERROR_OUT_OF_MEMORY.

    @def PVR_LOGG_IF_NOMEM
    @brief Logs error if expression is NULL and jumps to given label.

    @def PVR_LOGR_IF_ERROR
    @brief Logs error if not PVRSRV_OK and returns the error.

    @def PVR_LOGRN_IF_ERROR
    @brief Logs error if not PVRSRV_OK and returns (used in function that return void).

    @def PVR_LOGG_IF_ERROR
    @brief Logs error if not PVRSRV_OK and jumps to label.

    @def PVR_LOG_IF_FALSE
    @brief Prints error message if expression is false.

    @def PVR_LOGR_IF_FALSE
    @brief Prints error message if expression is false and returns given error.

    @def PVR_LOGG_IF_FALSE
    @brief Prints error message if expression is false and jumps to label.

    @def PVR_RETURN_IF_ERROR
    @brief Returns passed error code if it's different than PVRSRV_OK;

    @def PVR_LOG_WARN
    @brief Logs warning.

    @def PVR_LOG_WARN_IF_ERROR
    @brief Logs warning if not PVRSRV_OK.

    @def PVR_LOG
    @brief Prints message to a log unconditionally.

    This macro will print messages only if PVRSRV_NEED_PVR_LOG macro is defined.

    @def PVR_TRACE_EMPTY_LINE
    @brief Prints empty line to a log (PVRSRV_NEED_PVR_LOG must be defined).

    @def TRUNCATE_64BITS_TO_32BITS
    @brief Truncates 64 bit value to 32 bit value (with possible precision loss).

    @def TRUNCATE_64BITS_TO_SIZE_T
    @brief Truncates 64 bit value to size_t value (with possible precision loss).

    @def TRUNCATE_SIZE_T_TO_32BITS
    @brief Truncates size_t value to 32 bit value (with possible precision loss).
 */

#endif	/* PVR_DEBUG_H */

/******************************************************************************
 End of file (pvr_debug.h)
******************************************************************************/
