#pragma once

#include <cstdio>
#include <string>
#include <map>

#include <csignal>
#include <cstring>
#include <unistd.h>
#include <execinfo.h>
#include <cxxabi.h>
#include <cstdlib>
#include <sstream>
#include <iostream>
#include <thread>
#include <atomic>
#include <map>
#include <mutex>

#if (defined(WIN32) || defined(_WIN32) || defined(__WIN32__) && !defined(__GNUC__))
#define DISABLE_FATAL_SIGNALHANDLING 1
#endif

// Linux/Clang, OSX/Clang, OSX/gcc
#if (defined(__clang__) || defined(__APPLE__))
#include <sys/ucontext.h>
#else
#include <ucontext.h>
#endif

namespace spdlog
{
namespace details
{
    typedef int SignalType;

namespace internal {
    void restoreSignalHandler(int signal_number) {
#if !(defined(DISABLE_FATAL_SIGNALHANDLING))
      struct sigaction action;
      memset(&action, 0, sizeof (action)); //
      sigemptyset(&action.sa_mask);
      action.sa_handler = SIG_DFL; // take default action for the signal
      sigaction(signal_number, &action, NULL);
#endif
   }

      /** return whether or any fatal handling is still ongoing
       *  this is used by g3log::fatalCallToLogger
       *  only in the case of Windows exceptions (not fatal signals)
       *  are we interested in changing this from false to true to
       *  help any other exceptions handler work with 'EXCEPTION_CONTINUE_SEARCH'*/
      bool shouldBlockForFatalHandling()
      {
          return true;  // For windows we will after fatal processing change it to false
      }

      /** \return signal_name Ref: signum.hpp and \ref installSignalHandler
      *  or for Windows exception name */
      std::string exitReasonName(SignalType fatal_id)
      {
          int signal_number = static_cast<int>(fatal_id);
         switch (signal_number) {
            case SIGABRT: return "SIGABRT";
               break;
            case SIGFPE: return "SIGFPE";
               break;
            case SIGSEGV: return "SIGSEGV";
               break;
            case SIGILL: return "SIGILL";
               break;
            case SIGTERM: return "SIGTERM";
               break;
            default:
               std::ostringstream oss;
               oss << "UNKNOWN SIGNAL(" << signal_number << ")";// for " << level.text;
               return oss.str();
         }
      }

      /** return calling thread's stackdump*/
      std::string stackdump(const char* rawdump = nullptr)
      {
             if (nullptr != rawdump && !std::string(rawdump).empty()) {
            return {rawdump};
         }

         const size_t max_dump_size = 50;
         void* dump[max_dump_size];
         size_t size = backtrace(dump, max_dump_size);
         char** messages = backtrace_symbols(dump, static_cast<int>(size)); // overwrite sigaction with caller's address

         // dump stack: skip first frame, since that is here
         std::ostringstream oss;
         for (size_t idx = 1; idx < size && messages != nullptr; ++idx) {
            char* mangled_name = 0, *offset_begin = 0, *offset_end = 0;
            // find parantheses and +address offset surrounding mangled name
            for (char* p = messages[idx]; *p; ++p) {
               if (*p == '(') {
                  mangled_name = p;
               } else if (*p == '+') {
                  offset_begin = p;
               } else if (*p == ')') {
                  offset_end = p;
                  break;
               }
            }

            // if the line could be processed, attempt to demangle the symbol
            if (mangled_name && offset_begin && offset_end &&
                  mangled_name < offset_begin) {
               *mangled_name++ = '\0';
               *offset_begin++ = '\0';
               *offset_end++ = '\0';

               int status;
               char* real_name = abi::__cxa_demangle(mangled_name, 0, 0, &status);
               // if demangling is successful, output the demangled function name
               if (status == 0) {
                  oss << "\n\tstack dump [" << idx << "]  " << messages[idx] << " : " << real_name << "+";
                  oss << offset_begin << offset_end << std::endl;
               }// otherwise, output the mangled function name
               else {
                  oss << "\tstack dump [" << idx << "]  " << messages[idx] << mangled_name << "+";
                  oss << offset_begin << offset_end << std::endl;
               }
               free(real_name); // mallocated by abi::__cxa_demangle(...)
            } else {
               // no demangling done -- just dump the whole line
               oss << "\tstack dump [" << idx << "]  " << messages[idx] << std::endl;
            }
         } // END: for(size_t idx = 1; idx < size && messages != nullptr; ++idx)
         free(messages);
         return oss.str();
      }

      /** Re-"throw" a fatal signal, previously caught. This will exit the application
       * This is an internal only function. Do not use it elsewhere. It is triggered
       * from g3log, g3LogWorker after flushing messages to file */
      void exitWithDefaultSignalHandler(SignalType fatal_signal_id)
      {
          const int signal_number = static_cast<int>(fatal_signal_id);
         restoreSignalHandler(signal_number);
         std::cerr << "\n\n" << __FUNCTION__ << ":" << __LINE__ << ". Exiting due to signal " << ", " << signal_number << "   \n\n" << std::flush;


         kill(getpid(), signal_number);
         exit(signal_number);
      }

      const std::map<int, std::string> kSignals = {
      {SIGABRT, "SIGABRT"},
      {SIGFPE, "SIGFPE"},
      {SIGILL, "SIGILL"},
      {SIGSEGV, "SIGSEGV"},
      {SIGTERM, "SIGTERM"},
   };

   std::map<int, std::string> gSignals = kSignals;

   bool shouldDoExit() {
      static std::atomic<uint64_t> firstExit{0};
      auto const count = firstExit.fetch_add(1, std::memory_order_relaxed);
      return (0 == count);
   }

   // Dump of stack,. then exit through g3log background worker
   // ALL thanks to this thread at StackOverflow. Pretty much borrowed from:
   // Ref: http://stackoverflow.com/questions/77005/how-to-generate-a-stacktrace-when-my-gcc-c-app-crashes
   void signalHandler(int signal_number, siginfo_t* info, void* unused_context) {

      // Only one signal will be allowed past this point
      if (false == shouldDoExit()) {
         while (true) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
         }
      }

      //using namespace spdlog::details::internal;
      {
         const auto dump = stackdump();
         std::ostringstream fatal_stream;
         const auto fatal_reason = exitReasonName(signal_number);
         fatal_stream << "Received fatal signal: " << fatal_reason;
         fatal_stream << "(" << signal_number << ")\tPID: " << getpid() << std::endl;
         fatal_stream << "\n***** SIGNAL " << fatal_reason << "(" << signal_number << ")" << std::endl;
         //LogCapture trigger(FATAL_SIGNAL, static_cast<SignalType>(signal_number), dump.c_str());
         //trigger.stream() << fatal_stream.str();
      } // message sent to g3LogWorker
      // wait to die
   }



   //
   // Installs FATAL signal handler that is enough to handle most fatal events
   //  on *NIX systems
   void installSignalHandler() {
#if !(defined(DISABLE_FATAL_SIGNALHANDLING))
      struct sigaction action;
      memset(&action, 0, sizeof (action));
      sigemptyset(&action.sa_mask);
      action.sa_sigaction = &signalHandler; // callback to crashHandler for fatal signals
      // sigaction to use sa_sigaction file. ref: http://www.linuxprogrammingblog.com/code-examples/sigaction
      action.sa_flags = SA_SIGINFO;

      // do it verbose style - install all signal actions
      for (const auto& sig_pair : gSignals) {
         if (sigaction(sig_pair.first, &action, nullptr) < 0) {
            const std::string error = "sigaction - " + sig_pair.second;
            perror(error.c_str());
         }
      }
#endif
   }
   } // end internal

// PUBLIC API:
   /** Install signal handler that catches FATAL C-runtime or OS signals
     See the wikipedia site for details http://en.wikipedia.org/wiki/SIGFPE
     See the this site for example usage: http://www.tutorialspoint.com/cplusplus/cpp_signal_handling
     SIGABRT  ABORT (ANSI), abnormal termination
     SIGFPE   Floating point exception (ANSI)
     SIGILL   ILlegal instruction (ANSI)
     SIGSEGV  Segmentation violation i.e. illegal memory reference
     SIGTERM  TERMINATION (ANSI)  */
   void installCrashHandler()
   {
        internal::installSignalHandler();
   }

   /// Probably only needed for unit testing. Resets the signal handling back to default
   /// which might be needed in case it was previously overridden
   /// The default signals are: SIGABRT, SIGFPE, SIGILL, SIGSEGV, SIGTERM
   void restoreSignalHandlerToDefault()
   {
       overrideSetupSignals(kSignals);
   }

   /// Overrides the existing signal handling for custom signals
   /// For example: usage of zcmq relies on its own signal handler for SIGTERM
   ///     so users of g3log with zcmq should then use the @ref overrideSetupSignals
   ///     , likely with the original set of signals but with SIGTERM removed
   /// 
   /// call example:
   ///  g3::overrideSetupSignals({ {SIGABRT, "SIGABRT"}, {SIGFPE, "SIGFPE"},{SIGILL, "SIGILL"},
   //                          {SIGSEGV, "SIGSEGV"},});
   void overrideSetupSignals(const std::map<int, std::string> overrideSignals)
   {
       static std::mutex signalLock;
      std::lock_guard<std::mutex> guard(signalLock);
      for (const auto& sig : gSignals) {
         restoreSignalHandler(sig.first);
      }

      gSignals = overrideSignals;
      installCrashHandler(); // installs all the signal handling for gSignals
   }
} // end details
} // end spdlog