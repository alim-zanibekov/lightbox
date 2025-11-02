import type { LightboxApi, MatrixData, GalleryItem, Settings, Gallery, FirmwareInfo } from "./LightboxApi"

export class MockLightboxApi implements LightboxApi {
  private matrix: MatrixData = {
    matrix: Array.from(
      { length: 20 },
      (_, i) => i === 0 || i === 19
        ? Array.from({ length: 15 }, () => "#FF0000")
        : Array.from({ length: 15 }, (_, i) => i === 0 || i === 14 ? "#FF0000" : "#FF00FF")
    ),
    brightness: 100,
  }

  private gallery: GalleryItem[] = []

  private settings: Settings = {
    staSsid: "HomeWiFi",
    staPassword: "password123",
    staEnabled: true,
    staFallbackToApTimeout: 30,
    apSsid: "LightboxAP",
    apPassword: "12345678",
    hostname: "lightbox",
    deviceName: "My Lightbox",
    apAlwaysEnabled: false,
    apSsidHidden: false,
  }

  private delay = async (ms = 500) => new Promise(
    (res) => setTimeout(res, ms)
  )

  getMatrix = async (): Promise<MatrixData> => {
    await this.delay(1000)
    return JSON.parse(JSON.stringify(this.matrix))
  }

  postMatrix = async (data: MatrixData): Promise<MatrixData> => {
    await this.delay()
    this.matrix = data
    return JSON.parse(JSON.stringify(this.matrix))
  }

  getGallery = async (): Promise<Gallery> => {
    await this.delay()
    return {
      capacity: 20,
      items: JSON.parse(JSON.stringify(this.gallery)),
    }
  }

  postGallery = async (item: GalleryItem): Promise<GalleryItem> => {
    await this.delay()
    this.gallery.push(item)
    return JSON.parse(JSON.stringify(item))
  }

  deleteGallery = async (index?: number): Promise<{ success: boolean }> => {
    await this.delay()
    if (index !== undefined) this.gallery.splice(index, 1)
    else this.gallery = []
    return { success: true }
  }

  getSettings = async (): Promise<Settings> => {
    await this.delay()
    return JSON.parse(JSON.stringify(this.settings))
  }

  putSettings = async (partial: Partial<Settings>): Promise<Settings> => {
    await this.delay()
    this.settings = { ...this.settings, ...partial }
    return JSON.parse(JSON.stringify(this.settings))
  }

  getFirmwareInfo = async (): Promise<FirmwareInfo> => {
    await this.delay()
    return {
      version: "1.0.0",
      hashSHA256: "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855",
      magicWord: 0xCAFEBABE,
      compileDate: "2025-10-13",
      compileTime: "12:00:00"
    }
  }

  // eslint-disable-next-line @typescript-eslint/no-unused-vars
  otaUpdate = async (_: File, __?: (progress: number) => void): Promise<void> => {
    await this.delay()
    throw new Error("Unable to update lightbox")
  }
}
