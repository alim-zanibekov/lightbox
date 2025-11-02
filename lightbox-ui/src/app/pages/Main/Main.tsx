import React from "react"
import styles from "./Main.module.css"
import { PainSection } from "./PaintPanel.tsx"
import { GalleryPanel } from "./GalleryPanel.tsx"
import { CanvasPanel } from "./CanvasPanel.tsx"
import { SettingsPanel } from "./SettingsPanel.tsx"
import { FirmwarePanel } from "./FirmwarePanel.tsx"

const glassPanelClasses = "backdrop-blur-lg rounded-xl p-4 border bg-white/30 dark:bg-black/30 border-black/10 dark:border-white/20"

export const Main: React.FC = () => {
  return (
    <div className={styles.layout}>
      <section className={`${styles.grid} grid min-h-dvh bg-gradient-to-br p-4 overflow-hidden from-gray-700
        via-gray-300 to-gray-500 dark:from-slate-900 dark:via-gray-800 dark:to-slate-900 min-w-fit`}>

        <div className={`${glassPanelClasses} flex flex-col`} style={{ gridArea: "canvas" }}>
          <CanvasPanel></CanvasPanel>
        </div>

        <div className="flex-col gap-4 hidden lg:flex" style={{ gridArea: "paint-gallery" }}>
          <div className={`${glassPanelClasses}`} style={{ gridArea: "paint" }}>
            <PainSection/>
          </div>
          <div className={`${glassPanelClasses}`} style={{ gridArea: "gallery" }}>
            <GalleryPanel></GalleryPanel>
          </div>
          <div className={`${glassPanelClasses} flex justify-center gap-2`} style={{ gridArea: "gallery" }}>
            <SettingsPanel></SettingsPanel>
            <FirmwarePanel></FirmwarePanel>
          </div>
        </div>

        <div className={`${glassPanelClasses} pr-2! pl-2! flex gap-2 lg:hidden overflow-x-auto overflow-y-hidden`} style={{ gridArea: "settings" }}>
          <GalleryPanel></GalleryPanel>
          <PainSection/>
          <SettingsPanel></SettingsPanel>
          <FirmwarePanel></FirmwarePanel>
        </div>
      </section>
    </div>
  )
}
