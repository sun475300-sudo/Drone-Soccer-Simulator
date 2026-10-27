# 물리 모델과 보정 범위

2026-10-26 작성,2026-10-27 ANGLE·골대·전력·조종기 설명 대조 갱신. 현재 계수는 실험용 가정값이다. 특정 STRIKER V3의 실측 디지털 트윈이나 FIDA 인증을 뜻하지 않는다. 기본값은 `Content/Data/physics-defaults.json`에서 수정하고 다시 패키징한다. 엔진 독립 계산은 `PhysicsModels.{h,cpp}`, 실제 힘 적용은 `SoccerDronePawn.cpp`에 있다. 후속 순서는 [통합 개발 계획서](master-development-plan.md#physics), 버전별 근거는 [검증 기록](verification.md)을 따른다.

| 요청 항목 | 게임에 적용한 방법 | 범위와 한계 |
|---|---|---|
| 뉴턴–오일러 강체 | Chaos 중력·힘·접촉·각속도 적분, 네 모터 위치에서 힘과 반작용 토크 | Chaos 강체 자이로 옵션을 명시적으로 켜 회전축 결합을 적용. 별도 중복 적분 없음 |
| 질량·무게중심·관성 | 보호구/배터리/프레임/전자부품/네 모터 질량을 합산하고 평행축 정리로 관성 계산 | 총질량 0.9kg, 무게중심 약(0,0,-7.50)mm. 전체 텐서를 계산하지만 Chaos에는 대각 성분만 적용 |
| PWM·ESC·포화 | `T = voltageFactor*(Tidle+(Tmax-Tidle)*PWM^2)`, 35ms 일차 응답, 모터별 제한 | 지수·응답·최대 추력 8N은 실측값 아님. 자율 제어기는 역곡선으로 뉴턴 단위 요구 추력을 PWM으로 변환 |
| 항력 | `F=-0.5*rho*CdA*\|v-air\|*(v-air)` | rho=1.225kg/m³, 유효 CdA=0.015m². 속이 찬 구의 Cd를 그대로 쓰지 않음. 현재 등방성 근사이며 자세별 Cd와 독립 로터 항력 계수는 미보정 |
| 충돌·마찰 | 반경20cm 구체 CCD, 바닥/골대/보호구/보조벽의 다른 정지·동마찰/반발계수 | 격자 봉 개별 접촉·파손·얽힘은 미구현. 골대는 `ImperialSuspendedGoal`의 접촉 흔들림/감쇠와 경기 안정화 모드를 사용 |
| 보호구 탄성 | 충격량에 따른 제한된 가시 압축·감쇠 복원, 반발/마찰로 충격 손실 | 보호구 격자를 실제 변형 충돌 메시로 푸는 연성체 해석이 아님. 구형 충돌 반경은 고정 |
| 네트 탄성·감쇠 | 명목 네트 면에 구체가 닿으면 `max(0,k*penetration-c*separatingVelocity)` 힘을 접촉점에 적용 | k=300N/m,c=22Ns/m,Fmax180N. 분포 장력 해석 대신 국소 스프링 근사. 최대20cm 시각 처짐과 뒤쪽 보조 충돌벽 |
| 네트 표현 | 10cm 격자·12,934개 분할 와이어, 접촉 주변 가우시안 변형·고정 가장자리·감쇠 복원 | 표시 액터는 힘을 중복 적용하지 않음. 기체의 실제 접촉 모델로 구동 |
| 지면·천장·벽 | 각 로터에서 표면 거리와 로터 반지름에 따른 제한된 추력 보정 | 4로터 반지름 이내에서 작용하는 경험적 근사. 벽면 유동과 실험 계수를 정확히 재현하지 않음 |
| 다운워시 | 실제 다른 기체의 위치·합산 추력에서 하향 가우시안 유동을 계산, 상대 항력·로터별 추력 손실에 적용 | 거리 감쇠·원뿔 확산·최대 풍속 제한. 수직축 후류 근사이며 CFD·학습된 Neural-Swarm 모델은 아님 |
| 로터 자이로 | 회전자 각운동량과 몸체 각속도의 교차곱 토크 | 로터 관성은 가정값. 회전수 변화에 따른 `-dH/dt` 항은 아직 별도로 적용하지 않음 |
| 배터리 | 용량 적분·개방전압·내부저항·전류 상한과 공급 전력 예산을 실제 모터 힘/토크에 연결 | v0.7.4 전력 제한 검사 근거 있음. 4S·1.5Ah·0.035Ω은 가정, 온도·개별 셀·화학적 회복·실측 보정은 후속 |
| 제어 | 각속도 PID, 미분 필터, 적분 제한·back-calculation, 믹서 포화 시 적분 감쇠, 무게중심 토크 보상 | 수동은 ANGLE35도·중립 수평 복원, Yaw/스로틀 수동. 자율 훈련만 위치/속도/자세 계단식 제어. v0.7.6-dev 축별 레이트/Expo는 [응답 문서](controller-response-2026-10-27.md) 참조 |
| 센서 | 몸체 좌표 자이로·특정힘(a-g), 재현 가능한 잡음·바이어스·5ms 지연 큐 | PID는 지연된 자이로를 사용. 가속도계는 센서 모델 출력이며 상태추정기/센서 융합은 미구현. 별도 회전자 진동 스펙트럼 미보정 |
| 조종기 | WinMM이 보고한 Windows 조이스틱 입력·범위/중립/반전/데드존, 네 축 중복 거부, 낮은 스로틀 ARM, 연결/포커스 상실 DISARM | Mode1/2·응답/아밍키 소프트웨어 검사와 실물 검증을 구분. 현재6축/32버튼, 범용 입력 확대·연속 실물 비행·입력-화면 지연 측정은 후속 |
| 수치 시간 | PrePhysics 입력·제어·ESC 갱신, Chaos 최대1/120초·8서브스텝, CCD | **제어기는 독립 고정 물리 스레드가 아니다.** 느린 프레임에서 입력/센서/PID 갱신도 느려지며 고정주기 제어는 후속 작업 |

무게중심 오프셋은 루트 스케일0.4를 역보정해 UE에 전달한다. `InertiaTensorScale`은 관성 성분 배수가 아닌 형상 축 스케일이므로 사용하지 않고, 질량 설정 후 Chaos에 원하는 관성 대각을 직접 넣는다. 시작 로그 `IMPERIAL_MASS`에서 실제 질량·무게중심·관성 읽기값을 확인한다. 초기화 중 `SetMassOverride`나 `UpdateMassProperties`를 뒤에 추가하면 이 설정이 덮어써질 수 있다.

일반 이동에는 텔레포트를 사용하지 않는다. HOME/Backspace 초기화와 명시적인 자동 시험 fixture의 첫 배치·충돌 초기 속도만 예외다. `-ImperialBaselinePhysics`는 배터리·비선형 추력·표면/후류/센서 보정을 끄는 비교용 실행 옵션이다. 이것도 별도 실제 기체 인증 모드가 아니다.

## 보정할 측정

호버 스로틀·전력, PWM-추력·RPM 응답, 축별 감속, 무게중심·관성, 자유낙하·바닥 반발, 기체 충돌, 5m/s 네트 충돌, 표면 근접 호버, 위아래 두 기체 간 간섭 순서로 측정값을 확보한다. 현재 네트 시험과 호버/목표 이동/실제 골대 통과 결과는 [검증 기록](verification.md)에 남긴다. 독립 계산 시험의 통과가 실기체 정확도를 증명하지 않는다.

## 공식·연구 근거

- [MIT Quadrotor Dynamics](https://vnav.mit.edu/material/06-Control1-notes.pdf): 강체 힘·모멘트와 로터 모델 참고. 설명서 원문을 저장소에 복제하지 않는다.
- [ArduPilot Motor Thrust Scaling](https://ardupilot.org/copter/docs/motor-thrust-scaling.html): ESC/모터 비선형성과 추력 실측 보정 필요성. 현재 제곱 곡선이 ArduPilot 기본 EXPO 곡선과 같다는 뜻은 아니다.
- [NASA 항력식](https://www1.grc.nasa.gov/beginners-guide-to-aeronautics/modern-drag-equation/): 상대 유속·밀도·형상 계수에 따른 항력.
- [Faessler et al., rotor drag](https://arxiv.org/abs/1712.02402): 로터 항력을 고려한 추적 연구. 현재 모델에 논문의 전체 제어기를 이식하지 않았다.
- [Epic Physical Materials](https://dev.epicgames.com/documentation/en-us/unreal-engine/physical-materials-reference-for-unreal-engine): 마찰·반발 및 접촉 재질 결합.
- [MuJoCo computation](https://mujoco.readthedocs.io/en/latest/computation/index.html): 접촉·마찰 모델의 참고. 실제 런타임 솔버는 Chaos다.
- [Neural-Swarm2](https://arxiv.org/abs/2012.05457): 가까운 기체의 공기 간섭을 다룬 연구. 현재 하향류 모델은 별도의 간략 근사다.
- [Epic Physics Sub-Stepping](https://dev.epicgames.com/documentation/en-us/unreal-engine/physics-sub-stepping-in-unreal-engine): 프레임 내 솔버 분할과 지속 힘 처리. 서브스텝을 고정주기 PID로 부르지 않는다.

사용자가 제시한 표면 근접 효과 자료의 대학 저장소는 확인 시403응답이어서 구체 계수의 근거로 사용하지 않았다. 현재 근접 보정 계수는 실험값으로 표기한다.
