# AssureCheck — Assurance Standards Checklists

Reference checklists for the AssureCheck module. Each standard has objectives/checklist
items. Each item carries an id, description, applicable DAL (Development Assurance Level)
where relevant, and the evidence required. These are the seed data for the AssureCheck
compliance engine.

DAL levels (DO-178C / DO-254 / ARP4754A): **A** (catastrophic) · **B** (hazardous) ·
**C** (major) · **D** (minor) · **E** (no safety effect). A check marked "A–D" applies to
DAL A through D; "A" alone applies only to DAL A.

---

## 1. DO-178C — Software Considerations in Airborne Systems and Equipment Certification

### Table A-1 — Software Planning Process
| ID | Objective | DAL | Evidence |
|----|-----------|-----|----------|
| A1-1 | Software life cycle processes are defined | A–D | PSAC (Plan for Software Aspects of Certification) |
| A1-2 | Software life cycle data is defined | A–D | SDD (Software Development Plan) |
| A1-3 | Software life cycle environment is defined | A–D | SDP (Software Development Plan) |
| A1-4 | Software development standards are defined | A–D | SDP / coding standards |
| A1-5 | Software plans comply with DO-178C | A–D | Plan review |
| A1-6 | Software plans are coordinated with system plans | A–D | Plan review |
| A1-7 | Software plans are reviewed and approved | A–D | Plan approval records |
| A1-8 | Software standards are reviewed and approved | A–D | Standards approval records |
| A1-9 | Software plans are under configuration management | A–D | CM records |
| A1-10 | Software plans are under process assurance | A–D | Process assurance records |

### Table A-2 — Software Development Processes
| ID | Objective | DAL | Evidence |
|----|-----------|-----|----------|
| A2-1 | High-level requirements are developed | A–D | HL requirements |
| A2-2 | High-level requirements are verifiable | A–D | HL requirements review |
| A2-3 | High-level requirements conform to standards | A–D | Standards compliance |
| A2-4 | High-level requirements are traceable to system requirements | A–D | Traceability matrix |
| A2-5 | Low-level requirements are developed | A–D | LL requirements |
| A2-6 | Low-level requirements are verifiable | A–D | LL requirements review |
| A2-7 | Low-level requirements conform to standards | A–D | Standards compliance |
| A2-8 | Low-level requirements are traceable to high-level requirements | A–D | Traceability matrix |
| A2-9 | Software architecture is developed | A–D | Architecture description |
| A2-10 | Software architecture is verifiable | A–D | Architecture review |
| A2-11 | Software architecture conforms to standards | A–D | Standards compliance |
| A2-12 | Software architecture is consistent with high-level requirements | A–D | Consistency review |
| A2-13 | Software architecture is partitioned | A–C | Partitioning evidence |
| A2-14 | Software architecture is traceable to high-level requirements | A–D | Traceability matrix |
| A2-15 | Source code is developed | A–D | Source code |
| A2-16 | Source code is verifiable | A–D | Code review |
| A2-17 | Source code conforms to standards | A–D | Standards compliance |
| A2-18 | Source code is traceable to low-level requirements | A–D | Traceability matrix |
| A2-19 | Source code is consistent with low-level requirements | A–D | Consistency review |
| A2-20 | Source code is consistent with architecture | A–D | Consistency review |
| A2-21 | Source code complies with partitioning requirements | A–C | Partitioning evidence |
| A2-22 | Executable object code is developed | A–D | Build records |
| A2-23 | Executable object code is traceable to source code | A–D | Build traceability |
| A2-24 | Executable object code is consistent with source code | A–D | Build consistency |

### Table A-3 — Verification of Outputs of Software Requirements Process
| ID | Objective | DAL | Evidence |
|----|-----------|-----|----------|
| A3-1 | High-level requirements are accurate and complete | A–D | Requirements review |
| A3-2 | High-level requirements are compatible with target computer | A–D | Compatibility review |
| A3-3 | High-level requirements are verifiable | A–D | Verifiability review |
| A3-4 | High-level requirements conform to standards | A–D | Standards compliance |
| A3-5 | High-level requirements are traceable to system requirements | A–D | Traceability matrix |
| A3-6 | High-level requirements are consistent | A–D | Consistency review |
| A3-7 | High-level requirements are algorithmically accurate | A–C | Algorithm review |
| A3-8 | High-level requirements are compatible with low-level requirements | A–D | Compatibility review |

### Table A-4 — Verification of Outputs of Software Design Process
| ID | Objective | DAL | Evidence |
|----|-----------|-----|----------|
| A4-1 | Low-level requirements are accurate and complete | A–D | Requirements review |
| A4-2 | Low-level requirements are compatible with target computer | A–D | Compatibility review |
| A4-3 | Low-level requirements are verifiable | A–D | Verifiability review |
| A4-4 | Low-level requirements conform to standards | A–D | Standards compliance |
| A4-5 | Low-level requirements are traceable to high-level requirements | A–D | Traceability matrix |
| A4-6 | Low-level requirements are consistent | A–D | Consistency review |
| A4-7 | Low-level requirements are algorithmically accurate | A–C | Algorithm review |
| A4-8 | Software architecture is compatible with high-level requirements | A–D | Compatibility review |
| A4-9 | Software architecture is consistent | A–D | Consistency review |
| A4-10 | Software architecture conforms to standards | A–D | Standards compliance |
| A4-11 | Software architecture is compatible with target computer | A–D | Compatibility review |
| A4-12 | Software architecture is verifiable | A–D | Verifiability review |
| A4-13 | Software architecture is consistent with low-level requirements | A–D | Consistency review |
| A4-14 | Software partitioning is confirmed | A–C | Partitioning evidence |

### Table A-5 — Verification of Outputs of Software Coding Process
| ID | Objective | DAL | Evidence |
|----|-----------|-----|----------|
| A5-1 | Source code is accurate and complete | A–D | Code review |
| A5-2 | Source code is compatible with target computer | A–D | Compatibility review |
| A5-3 | Source code conforms to standards | A–D | Standards compliance |
| A5-4 | Source code is traceable to low-level requirements | A–D | Traceability matrix |
| A5-5 | Source code is consistent with low-level requirements | A–D | Consistency review |
| A5-6 | Source code is consistent with software architecture | A–D | Consistency review |
| A5-7 | Source code complies with partitioning requirements | A–C | Partitioning evidence |
| A5-8 | Source code is verifiable | A–D | Verifiability review |

### Table A-6 — Testing of Outputs of Integration Process
| ID | Objective | DAL | Evidence |
|----|-----------|-----|----------|
| A6-1 | Executable object code is robust with respect to high-level requirements | A–D | Test results |
| A6-2 | Executable object code is robust with respect to low-level requirements | A–D | Test results |
| A6-3 | Executable object code is compatible with target computer | A–D | Test results |
| A6-4 | High-level requirements are tested | A–D | Test results |
| A6-5 | Low-level requirements are tested | A–D | Test results |
| A6-6 | Software architecture is tested | A–D | Test results |
| A6-7 | Software partitioning is tested | A–C | Test results |
| A6-8 | Statement coverage is achieved | A–C | Coverage analysis |
| A6-9 | Decision coverage is achieved | A–B | Coverage analysis |
| A6-10 | MC/DC coverage is achieved | A | Coverage analysis |
| A6-11 | Data coupling and control coupling are analyzed | A–C | Coupling analysis |

### Table A-7 — Verification of Verification Process Results
| ID | Objective | DAL | Evidence |
|----|-----------|-----|----------|
| A7-1 | Test procedures are correct | A–D | Test procedure review |
| A7-2 | Test results are correct | A–D | Test result review |
| A7-3 | Test results are traceable to requirements | A–D | Traceability matrix |
| A7-4 | Test coverage is achieved | A–D | Coverage analysis |
| A7-5 | Verification results are complete | A–D | Verification review |
| A7-6 | Verification results are consistent | A–D | Consistency review |
| A7-7 | Verification results are under configuration management | A–D | CM records |

---

## 2. DO-254 — Design Assurance Guidance for Airborne Electronic Hardware

| ID | Objective | DAL | Evidence |
|----|-----------|-----|----------|
| D254-1 | Hardware planning is defined (PHAC, HDP, HVP, HCM, HPA) | A–D | Plans |
| D254-2 | Hardware requirements are developed | A–D | Hardware requirements |
| D254-3 | Hardware requirements are validated | A–D | Validation evidence |
| D254-4 | Hardware design is developed | A–D | Design data |
| D254-5 | Hardware design is verified | A–D | Verification evidence |
| D254-6 | Hardware implementation is developed | A–D | Implementation data |
| D254-7 | Hardware implementation is verified | A–D | Verification evidence |
| D254-8 | Hardware is tested | A–D | Test results |
| D254-9 | Hardware requirements are traceable to design | A–D | Traceability matrix |
| D254-10 | Hardware design is traceable to requirements | A–D | Traceability matrix |
| D254-11 | Hardware verification is complete | A–D | Verification review |
| D254-12 | Configuration management is applied | A–D | CM records |
| D254-13 | Process assurance is applied | A–D | Process assurance records |
| D254-14 | Certification liaison is established | A–D | Certification records |
| D254-15 | Hardware is robust to environmental conditions | A–D | Environmental test evidence |
| D254-16 | Hardware safety assessment is integrated | A–D | Safety assessment evidence |

---

## 3. ARP4754A — Development of Civil Aircraft and Systems

| ID | Objective | DAL | Evidence |
|----|-----------|-----|----------|
| A4754-1 | Development planning is established | A–D | Development plan |
| A4754-2 | Development assurance level is assigned | A–D | DAL assignment |
| A4754-3 | Aircraft/system functions are defined | A–D | Functional requirements |
| A4754-4 | System requirements are developed | A–D | System requirements |
| A4754-5 | System requirements are validated | A–D | Validation evidence |
| A4754-6 | System requirements are traceable to aircraft functions | A–D | Traceability matrix |
| A4754-7 | System architecture is developed | A–D | Architecture description |
| A4754-8 | System architecture is consistent with requirements | A–D | Consistency review |
| A4754-9 | System implementation is developed | A–D | Implementation data |
| A4754-10 | System requirements are verified | A–D | Verification evidence |
| A4754-11 | System verification is complete | A–D | Verification review |
| A4754-12 | Safety assessment is integrated with development | A–D | Safety assessment evidence |
| A4754-13 | Configuration management is applied | A–D | CM records |
| A4754-14 | Process assurance is applied | A–D | Process assurance records |
| A4754-15 | Certification evidence is produced | A–D | Certification records |
| A4754-16 | Requirements are traceable through the V-cycle | A–D | Traceability matrix |

---

## 4. ARP4761 — Guidelines and Methods for Conducting the Safety Assessment Process

| ID | Objective | DAL | Evidence |
|----|-----------|-----|----------|
| A4761-1 | Functional Hazard Assessment (FHA) is performed | A–D | FHA report |
| A4761-2 | Failure conditions are identified and classified | A–D | FHA report |
| A4761-3 | Preliminary System Safety Assessment (PSSA) is performed | A–D | PSSA report |
| A4761-4 | Safety requirements are allocated | A–D | PSSA report |
| A4761-5 | System Safety Assessment (SSA) is performed | A–D | SSA report |
| A4761-6 | Safety requirements are verified | A–D | SSA report |
| A4761-7 | Failure Modes and Effects Analysis (FMEA) is performed | A–D | FMEA report |
| A4761-8 | Fault Tree Analysis (FTA) is performed | A–D | FTA report |
| A4761-9 | Common Cause Analysis (CCA) is performed | A–D | CCA report |
| A4761-10 | Safety assessment is traceable to system requirements | A–D | Traceability matrix |
| A4761-11 | Safety assessment is complete and consistent | A–D | Safety review |

---

## 5. DO-278A — Software Aspects of Certification of Ground-Based Systems

| ID | Objective | DAL | Evidence |
|----|-----------|-----|----------|
| D278-1 | Ground system software planning is defined | A–D | Plans |
| D278-2 | Ground system software requirements are developed | A–D | Requirements |
| D278-3 | Ground system software requirements are verified | A–D | Verification evidence |
| D278-4 | Ground system software design is developed | A–D | Design data |
| D278-5 | Ground system software design is verified | A–D | Verification evidence |
| D278-6 | Ground system software code is developed | A–D | Source code |
| D278-7 | Ground system software code is verified | A–D | Verification evidence |
| D278-8 | Ground system software is tested | A–D | Test results |
| D278-9 | Ground system software is traceable to requirements | A–D | Traceability matrix |
| D278-10 | Configuration management is applied | A–D | CM records |
| D278-11 | Process assurance is applied | A–D | Process assurance records |

---

## Summary counts
- DO-178C: 10 (A-1) + 24 (A-2) + 8 (A-3) + 14 (A-4) + 8 (A-5) + 11 (A-6) + 7 (A-7) = **82 objectives**
- DO-254: **16 objectives**
- ARP4754A: **16 objectives**
- ARP4761: **11 objectives**
- DO-278A: **11 objectives**
- **Total: 136 checklist items** across the five standards.
