import type { LightboxApi, MatrixData, GalleryItem, Settings, Gallery, FirmwareInfo } from "./LightboxApi"

export class HttpLightboxApi implements LightboxApi {
  public baseUrl: string = "http://lightbox.local"

  constructor(baseUrl?: string) {
    if (baseUrl) this.baseUrl = baseUrl
  }

  private readMatrix(width: number, height: number, pixels: Uint8Array): string[][] {
    const expected = width * height * 3
    if (pixels.length < expected) throw new Error("Invalid LED data size")

    const matrix: string[][] = []

    let pos = 0
    for (let i = 0; i < height; i++) {
      const row: string[] = []
      for (let j = 0; j < width; j++) {
        const [r, g, b] = [pixels[pos++], pixels[pos++], pixels[pos++]]
        row.push(
          ("#" +
            r.toString(16).padStart(2, "0") +
            g.toString(16).padStart(2, "0") +
            b.toString(16).padStart(2, "0")).toUpperCase()
        )
      }
      matrix.push(row)
    }

    return matrix
  }

  private writeMatrix(matrix: string[][]): Uint8Array {
    const height = matrix.length
    const width = height ? matrix[0].length : 0
    const bytes = new Uint8Array(new ArrayBuffer(width * height * 3))
    let pos = 0
    for (let y = 0; y < height; y++) {
      for (let x = 0; x < width; x++) {
        const color = matrix[y][x]
        bytes[pos++] = parseInt(color.slice(1, 3), 16)
        bytes[pos++] = parseInt(color.slice(3, 5), 16)
        bytes[pos++] = parseInt(color.slice(5, 7), 16)
      }
    }
    return bytes
  }

  getMatrix = async (): Promise<MatrixData> => {
    const res = await fetch(`${this.baseUrl}/get_leds`)
    if (!res.ok) {
      const message = await res.json().then(it => it.error).catch(() => "Unknown error")
      throw new Error(`Failed to load LED matrix: ${message}`)
    }

    const buffer = await res.arrayBuffer()

    const data = new DataView(buffer)
    const brightness = data.getUint8(0)
    const width = data.getInt16(1, true)
    const height = data.getInt16(3, true)

    const pixels = new Uint8Array(buffer, 5, width * height * 3)

    const matrix = this.readMatrix(width, height, pixels)

    return { brightness, matrix }
  }

  postMatrix = async (data: MatrixData): Promise<MatrixData> => {
    const matrixBytes = this.writeMatrix(data.matrix)
    const bytes = new Uint8Array(1 + matrixBytes.length)
    bytes.set([data.brightness], 0)
    bytes.set(matrixBytes, 1)

    const res = await fetch(`${this.baseUrl}/set_leds`, {
      method: "POST",
      headers: { "Content-Type": "application/octet-stream" },
      body: bytes.buffer,
    })

    if (!res.ok) {
      const message = await res.json().then(it => it.error).catch(() => "Unknown error")
      throw new Error(`Failed to send LED data: ${message}`)
    }
    return data
  }

  getGallery = async (): Promise<Gallery> => {
    const res = await fetch(`${this.baseUrl}/get_gallery`)
    if (!res.ok) {
      const message = await res.json().then(it => it.error).catch(() => "Unknown error")
      throw new Error(`Failed to load gallery: ${message}`)
    }

    const buffer = await res.arrayBuffer()

    const data = new DataView(buffer)
    const capacity = data.getUint16(0, true)
    const count = data.getUint16(2, true)
    const width = data.getInt16(4, true)
    const height = data.getInt16(6, true)

    let offset = 8

    const items = [] as GalleryItem[]
    for (let i = 0; i < count; i++) {
      const nameLen = data.getUint16(offset, true)
      offset += 2
      const name = new Uint8Array(buffer, offset).slice(0, nameLen)
      offset += nameLen
      const pixels = new Uint8Array(buffer, offset, width * height * 3)
      const matrix = this.readMatrix(width, height, pixels)
      items.push({ name: new TextDecoder().decode(name), matrix })
      offset += pixels.length
    }

    return {
      capacity,
      items,
    }
  }

  postGallery = async (item: GalleryItem): Promise<GalleryItem> => {
    const matrixBytes = this.writeMatrix(item.matrix)
    const name = new TextEncoder().encode(item.name)
    const bytes = new Uint8Array(2 + name.length + matrixBytes.length)
    const data = new DataView(bytes.buffer)
    data.setUint16(0, name.length, true)
    bytes.set(name, 2)
    bytes.set(matrixBytes, 2 + name.length)

    const res = await fetch(`${this.baseUrl}/post_gallery`, {
      method: "POST",
      headers: { "Content-Type": "application/octet-stream" },
      body: bytes.buffer,
    })

    if (!res.ok) {
      const message = await res.json().then(it => it.error).catch(() => "Unknown error")
      throw new Error(`Failed to save: ${message}`)
    }

    return item
  }

  deleteGallery = async (index?: number): Promise<{ success: boolean }> => {
    const url = index !== undefined ? `${this.baseUrl}/delete_gallery?index=${index}` : `${this.baseUrl}/delete_gallery`
    const res = await fetch(url, { method: "DELETE" })
    if (!res.ok) {
      const message = await res.json().then(it => it.error).catch(() => "Unknown error")
      throw new Error(`Failed to delete from gallery: ${message}`)
    }
    return res.json()
  }

  getSettings = async (): Promise<Settings> => {
    const res = await fetch(`${this.baseUrl}/get_settings`)
    if (!res.ok) {
      const message = await res.json().then(it => it.error).catch(() => "Unknown error")
      throw new Error(`Failed to load settings: ${message}`)
    }
    return res.json()
  }

  putSettings = async (partial: Partial<Settings>): Promise<Settings> => {
    const res = await fetch(`${this.baseUrl}/put_settings`, {
      method: "PUT",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify(partial),
    })
    if (!res.ok) {
      const message = await res.json().then(it => it.error).catch(() => "Unknown error")
      throw new Error(`Failed to update settings: ${message}`)
    }
    return res.json()
  }

  getFirmwareInfo = async (): Promise<FirmwareInfo> => {
    const res = await fetch(`${this.baseUrl}/get_app_info`)
    if (!res.ok) {
      const message = await res.json().then(it => it.error).catch(() => "Unknown error")
      throw new Error(`Failed to load settings: ${message}`)
    }
    return res.json()
  }

  otaUpdate = (firmware: File, onProgress?: (progress: number) => void): Promise<void> => {
    return new Promise((resolve, reject) => {
      const xhr = new XMLHttpRequest()
      xhr.open("POST", `${this.baseUrl}/post_ota`)
      xhr.upload.onprogress = (event) => {
        if (event.lengthComputable && onProgress) {
          onProgress(Math.round((event.loaded / event.total) * 100))
        }
      }
      xhr.onload = () => {
        if (xhr.status >= 200 && xhr.status < 300) {
          resolve()
        } else {
          let message = "Unknown error"
          try {
            message = JSON.parse(xhr.responseText).error
            // eslint-disable-next-line @typescript-eslint/no-unused-vars
          } catch (_) { /* empty */
          }
          reject(new Error(`Failed to update firmware: ${message}`))
        }
      }
      xhr.onerror = () => {
        reject(new Error("Network error occurred during firmware update"))
      }
      xhr.send(firmware)
    })
  }
}
