# Remaining work

This list records user feedback and intentionally deferred work. It is not a list of completed fixes.

| Item | Evidence / scope | Next acceptance check |
| --- | --- | --- |
| Empty Spectrum and Waterfall without nRF24 | Expected with the current implementation; user no longer uses nRF24. | Decide whether to hide unavailable RF views or design a separate ESP32 Wi-Fi channel view. Do not relabel Wi-Fi RSSI as broadband RF spectrum. |
| Empty Wi-Fi discovery | User reports an empty list; cause not established. | Inspect scan lifecycle, mode exclusion, error codes and returned networks with the device idle. |
| Beacon capacity | Current implementation accepts at most four SSIDs. User requests 20–30 or unlimited. No capacity change in rc2. | Review finite memory and total transmission scheduling; measure dashboard responsiveness and independent receiver results before increasing capacity. Unlimited capacity cannot be guaranteed on finite hardware. |
| Beacon visibility | Earlier user tests did not see generated SSIDs. Latest screenshot shows driver acceptance and zero errors, not receiver proof. | Check reception on the actual channel during RUNNING with an independent receiver. |
| Whole-device acceptance | Upload and dashboard access reported; remaining features lack a complete physical test record. | Follow README checks and record each pass, failure or unavailable dependency. |

## Release scope

1.0.0-rc2 updates branding, embedded dashboard artwork, release identification and documentation. It does not repair or certify the deferred radio behavior. Existing control-node preservation remains in place.
