export type MatrixData = {
  matrix: string[][]
  brightness: number
}

export type Gallery = {
  capacity: number
  items: GalleryItem[]
}

export type GalleryItem = {
  matrix: string[][]
  name: string
}

export type FirmwareInfo = {
  version: string
  hashSHA256: string
  magicWord: number
  compileDate: string
  compileTime: string
}

export interface Settings {
  staFallbackToApTimeout: number;
  staSsid: string;
  staPassword: string;
  staEnabled: boolean;

  apAlwaysEnabled: boolean;
  apSsid: string;
  apSsidHidden: boolean;
  apPassword: string;

  hostname: string;
  deviceName: string;
}


export interface LightboxApi {
  getMatrix(): Promise<MatrixData>

  postMatrix(data: MatrixData): Promise<MatrixData>

  getGallery(): Promise<Gallery>

  postGallery(item: GalleryItem): Promise<GalleryItem>

  deleteGallery(index?: number): Promise<{ success: boolean }>

  getSettings(): Promise<Settings>

  putSettings(partial: Partial<Settings>): Promise<Settings>

  getFirmwareInfo(): Promise<FirmwareInfo>

  otaUpdate(firmware: File, onProgress?: (progress: number) => void): Promise<void>
}
