# CrashReporter External Aggregation

## Overview
CrashReporter now persists crash metadata and queue files so external services can aggregate reports reliably even if upload fails during crash time.

## What is generated on crash
- Dump file: %LOCALAPPDATA%/DirectX12/CrashDumps/Crash_<CrashId>.dmp
- Metadata file: %LOCALAPPDATA%/DirectX12/CrashDumps/Crash_<CrashId>.meta
- Queue file: %LOCALAPPDATA%/DirectX12/CrashQueue/<CrashId>.crashmeta

## Metadata fields
- schemaVersion
- crashId
- description
- dumpPath
- metadataPath
- exceptionCode
- exceptionAddress
- processId
- threadId
- timestampUtcMs

## Dispatch flow
1. Crash occurs.
2. Dump and metadata are persisted.
3. Queue file is persisted.
4. If external sink is registered, sink is invoked immediately and queue file is removed.
5. On next startup (initialize) or sink registration (setExternalSink), pending queue files are replayed and removed after successful sink call.

## Integration example
Register an external sink once at startup and upload metadata + dump to your backend.

Pseudo flow:
- CrashReporter::Instance().setExternalSink(...)
- sink reads report.dumpPath and report.metadataPath
- sink uploads multipart/form-data or API payload to crash backend

## Notes
- Queue replay marks report.recoveredFromQueue = true.
- Unsupported or malformed queue files are dropped with warning logs.
- setQueueDirectory can override default queue location.
