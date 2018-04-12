# CRETE: Versatile Binary-Level Concolic Testing

[![Build Status](https://travis-ci.org/SVL-PSU/crete-dev.svg?branch=master)](https://travis-ci.org/SVL-PSU/crete-dev)

`CRETE` is a versatile binary-level concolic testing framework developed by
the System Validation Lab at Portland State University. It can be applied to
various kinds of software systems for test case generation and bug detection,
including proprietary user-level programs, closed-source libraries, kernel
modules, etc.

## Highlights

* Open and extensible architecture: totally decoupled concrete and symbolic
  execution
* Standardized execution trace: llvm-based, self-contained, and composable
* Binary-level analysis: no source code or debug information required
* In-vivo analysis: use real full software stack and require no environment
  modeling
* Compact execution trace: selective binary-level tracing based on Dynamic Taint
  Analysis
* Trace/test selection and scheduling algorithms to improve effectiveness
* Simple usage model: no expertise required and accessible for general users

## Publications

* [1] Bo Chen, Christopher Havlicek, Zhenkun Yang, Kai Cong, Raghudeep Kannavara and
 Fei Xie. "CRETE: A Versatile Binary-Level Concolic Testing Framework".
 In: [The 21st International Conference on Fundamental Approaches to Software
 Engineering (FASE 2018)](https://www.etaps.org/index.php/2018/fase),
 Thessaloniki, Greece, April 2018.
 [[LINK](https://link.springer.com/chapter/10.1007/978-3-319-89363-1_16)]
 [[PDF](https://link.springer.com/content/pdf/10.1007%2F978-3-319-89363-1.pdf)]
 [[BIB](https://citation-needed.springer.com/v2/references/10.1007/978-3-319-89363-1_16?format=bibtex&flavour=citation)]

## Support
If you need help with CRETE, or want to discuss the project, you can open new
[GitHub issues](https://github.com/SVL-PSU/crete-dev/issues/new), or contact the
[maintainer](https://github.com/likebreath).

We also have a very brief [user
manual](https://github.com/SVL-PSU/crete-dev/blob/master/user_manual.md), which
contains building instruction, running example, etc.