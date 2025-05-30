# Sierra Chart Studies

A collection of my custom Sierra Chart studies.

## Table of Contents

- [Installation](#installation)
- [AutoBAs - Automated Balance Area Detection](#autobas---automated-balance-area-detection)

---

## Installation

Place the `.cpp` file in `ACS_Source` folder, then go to **Analysis → Build Custom Studies DLL → Build → Remote Build - Standard.** Then **Analysis → Studies → Add Custom Study**

## AutoBAs - Automated Trading Levels

![AutoBAs Example](path/to/your/image.png)
*AutoBAs identifying balance areas with value area highlights and activation tracking*

AutoBAs is a market profile study that identifies and tracks Balance Areas (BAs) in real-time. The study implements a lifecycle management system that monitors BA formation, activation, and intersection dynamics.

### Core Functionality

The study analyzes consecutive trading sessions to identify periods where price remains balanced within defined ranges. It uses three detection methods: volume profile overlap between sessions, value area overlap, and geometric proximity analysis. When two sessions meet the criteria, a balance area is initiated and the study attempts to extend it by incorporating sessions that maintain similar characteristics.

Balance areas become "activated" when price breaks outside their value area boundaries. Once activated, the study draws extending rectangles that continue until they intersect with newer activated balance areas. This intersection management creates a hierarchy where older activated areas are "cut" by newer ones, generating Post-Balance Area Lines (PBALs) that serve as potential support or resistance levels.

### Features

The study includes statistical normality filtering using skewness and kurtosis calculations to identify distribution patterns and filter out noisy or incomplete balance areas. This helps focus attention on market structure developments.

Composite Balance Area detection identifies multi-session patterns, specifically High-Low-High (HLH) and Low-High-Low (LHL) formations. These patterns represent market structure developments and are validated through range containment, shift magnitude analysis, and temporal proximity checks.

Probe line detection highlights instances where price extends beyond balance area boundaries but returns, often indicating failed breakout attempts or areas of future significance.

### Configuration

The study requires a Volume by Price study as its data source and can track up to 500 trading sessions. Key parameters include volume overlap thresholds (typically 25-35%), value area overlap percentages, and geometric tolerance settings for range similarity and position validation.

Visual customization includes separate styling options for formation rectangles (typically blue), activated extending rectangles (typically orange), probe lines, and composite patterns. The study supports both automatic chart drawings and user-adjustable drawings for manual refinement.

Debug modes provide logging for balance area formation logic and composite detection, useful for parameter optimization and understanding the study's decision-making process.

### Application

AutoBAs identifies market structure in futures markets, particularly ES, NQ, and other liquid contracts with defined session boundaries. The activation tracking helps traders focus on relevant balance areas while the PBAL system provides reference levels.

The study performs continuous background analysis, making it suitable for both historical analysis and live trading applications. Its intersection management and statistical filtering help reduce noise while highlighting structural developments.

---
