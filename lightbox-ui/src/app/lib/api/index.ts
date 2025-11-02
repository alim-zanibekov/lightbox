import { MockLightboxApi } from "./MockLightboxApi"
import { HttpLightboxApi } from "./HttpLightboxApi"
import type { LightboxApi } from "./LightboxApi"

export { type LightboxApi, MockLightboxApi, HttpLightboxApi }
//import.meta.env.MOCK_API
export const api = false
  ? new MockLightboxApi()
  : new HttpLightboxApi()
