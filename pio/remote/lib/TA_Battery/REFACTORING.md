# TA_Battery Refactoring Plan

## Changes:
- Namespace: `ta::battery` → `trailair::battery`
- Class: `TA_BatteryMonitor` → `BatteryMonitor`
- Member variables: Already use `_` suffix (correct style)
- Config struct: Already PascalCase (good)

## Files to update:
- TA_Battery.h
- TA_Battery.cpp
- Any usage sites in TA_RemoteApp

## Status: READY TO REFACTOR
