Automated Local Multi-Instance Execution (LaunchGame.bat).

Overview & Purpose. To facilitate local testing and multi-player validation without requiring multiple distinct physical machines, the project includes an automation script named "LaunchGame.bat". This script handles the concurrent startup of two distinct game instances (Player 1 and Player 2) directly on the same local PC, leveraging Unreal Engine's standalone launch options and DIS application indexing.

Implementation Mechanism.
The launchgame.bat script executes two separate instances of the packaged game executable with customized command-line arguments:Player 1 Instance: Launches the executable passing ?PlayerID=1 as an option string parameter.Player 2 Instance: Launches a second concurrent instance passing ?PlayerID=2.

Integration with Game Code Dynamic Role Assignment: During pawn spawning (ATankGameModeBase::SpawnDefaultPawnAtTransform_Implementation), the engine parses the PlayerID option from the launch string (UGameplayStatics::ParseOption).

DIS Application Mapping: The parsed ID dynamically assigns the local DIS ApplicationID via the ADISGameManager and configures the corresponding tank meshes and spawn transforms (using target points tagged as Player1 or Player2).
Network Loopback: Both instances communicate locally over the configured UDP port bindings (loopback interface), allowing full validation of DIS PDU transmissions, entity states, firing synchronization, and match state signaling (ATankGameStateBase) as if they were running across a distributed network.  

1. DIS Implementation Overview
The project integrates Distributed Interactive Simulation (DIS) protocols within Unreal Engine using the DISRuntime plugin alongside custom game managers (ATankGameDISGameManager, UTankGameInstance) and subsystem handlers (UUDPSubsystem, UPDUProcessor).
Core Entities: Tanks are mapped as DIS entities whose state and transforms are broadcasted and synchronized across the network.
Georeferencing: The project leverages the GeoReferencing plugin to handle real-world coordinate mapping and spatial positioning required by standard DIS environments.

2. Message / Event Flow
Communication relies on standard DIS Protocol Data Units (PDUs) mapped over UDP:
Entity State & Action PDUs: Entity transforms, orientation, and lifecycle events (such as firing and detonations via DISSendComponent and DISReceiveComponent) flow continuously between participating application nodes.
Custom Match Events (Signal PDU Payload): Match-level coordination, specifically round results, scoring, and resets, is encapsulated inside custom packet structures (FTankRoundResultPacket) and transmitted via DIS Signal PDUs. 
Processing Pipeline: Incoming packets are captured by the UPDUProcessor subsystem, filtered via a custom magic byte/payload check, and dispatched to ATankGameStateBase (HandleSignalPDUProcessed) to update scores, trigger visual destruction effects, or handle player respawns.

3. Authority Model
Due to the distributed nature of standard DIS where each peer operates as an independent authority over its own local simulation object:
Local Authority on Death: When a local tank is destroyed, only the dying client (owning authority of that entity) computes the round outcome, updates scores locally, and initiates the state broadcast.
Reliable Signaling via Resends: To mitigate potential packet loss inherent to raw UDP transmission of the round result, the authoritative node employs a timer-based resend mechanism (ResendTimerHandle, MaxResends, ResendInterval) to guarantee state convergence without requiring a centralized server.

4. Synchronization Approach
Entity Mapping: Remote tanks are filtered and registered via ATankGameStateBase::RegisterTank, identifying local controls versus remote reflections based on Application IDs and Entity IDs.
Ghost Projectiles: Projectile life cycles are synchronized using specialized tracking structures managed by UTankGameInstance (AddGhostProjectile, RemoveGhostProjectile), preventing desynchronization during fast-paced combat interactions.
Idempotency: State application functions (ApplyRoundResult) check incoming round numbers (LastAppliedRoundNumber) to ensure idempotency, safely ignoring duplicate or looped packet echoes.

5. Assumptions
P2P / Multi-Node Topology: The system assumes a cooperative or competitive peer-to-peer setup where each client runs an instance configured with distinct PlayerID launch arguments (parsed via OptionsString), mapping cleanly to DIS Application IDs.
Network Environment: Assumes a reliable local network or controlled testing environment where UDP packet loss is minimal and bounded by the implemented retry logic for critical events.

6. Limitations
UDP Packet Loss for Critical Events: While Signal PDU retries mitigate loss for round endings, raw entity state updates rely entirely on continuous streaming; severe jitter or packet drops can cause visual interpolation hiccups.
Hardcoded Entity Scaling: The current lookup relies on a strict 2-player assumption (CurrentPlayerId == 1 ? 2 : 1), limiting scalability to larger multi-team or multi-player sessions without architectural refactoring.
Lack of Dead Reckoning: Advanced trajectory prediction/dead reckoning algorithms for remote entities are minimal, relying mostly on direct state updates streamed through the DIS runtime.

7. What I Would Improve With More Time
Robust Dead Reckoning & Interpolation: Implement advanced dead reckoning models for tanks and projectiles to smooth out network latency and packet jitter.
Scalable Matchmaking / Multi-Player Support: Generalize the player indexing and scoring logic away from a hardcoded two-player model to support N-player free-for-all or team-based configurations.
Reliable Transport Layer for Game Events: Transition critical game-state triggers (score updates, match endings) to a reliable-ordered mechanism or implement explicit acknowledgment (ACK) packets instead of blind interval-based resends.
Configuration UI & Auto-Discovery: Build an in-game lobby system to dynamically configure Site IDs, Application IDs, and network endpoints without relying solely on command-line launch options.

Third Party AssetsI used Those third party assets alla downloaded from Fab:
Free Tanks models (T-34-85 and M4A3E8 Sherman) made by "KCISA"
Markov Pistol (PM) free model made by "TaigaForest"
Construction Site vol.1 free props made by "Dekogon Studios"
FXVarietyPack free particle system for hit and explosion effects

Note on AI Usage in the Project
During the development of TankGame, LLMs (specifically Claude and Gemini) were employed as collaborative technical assistants to streamline the implementation of the distributed simulation architecture and data-transfer mechanisms.DIS System Integration: AI models assisted in designing and refining the integration of the DISRuntime plugin within Unreal Engine, ensuring correct lifecycle management of game managers (ATankGameDISGameManager, ATankGameInstance) and state handlers.Data Transfer & Packaging: LLMs were used primarily to write, structure, and optimize custom data-transfer packages - such as the byte serialization/deserialization logic for FTankRoundResultPacket and its transmission via DIS Signal PDUs and UUDPSubsystem.Workflow Acceleration: They helped troubleshoot event propagation flows, timer-based resend mechanisms for unreliable UDP channels, and synchronization patterns between local and remote entities.  
