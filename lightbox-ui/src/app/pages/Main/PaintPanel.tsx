import { SteppedSlider } from "../../components/SteppedSlider.tsx"
import { Button } from "../../components/Button.tsx"
import React from "react"
import { Popover, PopoverButton, PopoverPanel } from "@headlessui/react"
import { ColorPicker } from "../../components/ColorPicker.tsx"
import iro from "@jaames/iro"
import { Paintbrush2 } from "lucide-react"
import { updateAppStore, useAppStore } from "../../store/appStore.ts"
import { GenericSuspense } from "../../components/GenericSuspense.tsx"

const popoverTransitionClasses = "transition duration-200 ease-out data-closed:translate-y-10 " +
  "data-closed:scale-95 data-open:translate-y-0 data-closed:opacity-0"

const glassPanelClasses = "backdrop-blur-lg rounded-xl p-4 border bg-white/50 dark:bg-black/50 border-black/10 dark:border-white/20"

export const PainSection = () => {
  return <>
    <div className="hidden lg:block">
      <SuspenseWrapper/>
    </div>
    <div className="lg:hidden">
      <Popover>
        <PopoverButton as="div">
          <Button size='sm'><Paintbrush2 size={16}/> Paint</Button>
        </PopoverButton>
        <PopoverPanel
          anchor={{ "to": "top start", "gap": "10px", "padding": "10px" }}
          transition
          className={`${glassPanelClasses} ${popoverTransitionClasses}`}>
          <SuspenseWrapper/>
        </PopoverPanel>
      </Popover>
    </div>
  </>
}

function SuspenseWrapper() {
  return (
    <div className="w-full">
      <div className="flex items-center justify-between">
        <div className="flex items-center gap-2 mb-4">
          <Paintbrush2 size={16} className="text-gray-700 dark:text-gray-300"/>
          <h2 className="text-sm font-semibold">Paint</h2>
        </div>
      </div>
      <GenericSuspense>
        <PaintControls/>
      </GenericSuspense>
    </div>
  )
}

export const PaintControls: React.FC = () => {
  const { matrix, paintColor, brightness, brushSize } = useAppStore((s) => ({
    matrix: s.matrix,
    brightness: s.brightness ?? 100,
    paintColor: s.paintColor,
    brushSize: s.brushSize,
  }))

  const colors = [
    "#FFFFFF", "#FF0000", "#00FF00", "#0000FF", "#BA55D3",
    "#F08080", "#2E8B57", "#DAA520", "#00BFFF", "#000000"
  ]

  return <>
    <h3 className="text-sm font-semibold text-gray-700 dark:text-gray-300 mb-2">Brightness</h3>
    <SteppedSlider
      step={1} min={1} max={100}
      disabled={!matrix}
      onChange={(brightness) => updateAppStore({ brightness })}
      value={brightness}
    />
    <h3 className="text-sm font-semibold text-gray-700 dark:text-gray-300 mb-2 mt-3">Brush color</h3>
    <div className="flex gap-2 items-center">
      <div className="grid grid-cols-5 gap-2 mb-2 mt-2 flex-1">
        {colors.map(color => (
          <button
            key={color}
            onClick={() => updateAppStore({ paintColor: color.toUpperCase() })}
            className={`w-10 aspect-square rounded-lg transition-all hover:scale-105 ${
              paintColor === color ? "ring-2 ring-blue-500" : "ring-0 ring-white/50"
            }`}
            style={{ backgroundColor: color }}
          />
        ))}
      </div>
      <ColorPopover
        paintColor={paintColor}
        setPaintColor={(color) => updateAppStore({ paintColor: color.toUpperCase() })}
      />
    </div>
    <div className="flex gap-2 items-center">
      <div className="w-full pr-2">
        <h3 className="text-sm font-semibold text-gray-700 dark:text-gray-300 mb-2">Brush size</h3>
        <SteppedSlider
          step={1} min={1} max={10}
          onChange={(brushSize) => updateAppStore({ brushSize })}
          defaultValue={brushSize}
        />
        <div><span className="text-gray-800 dark:text-white text-sm">{brushSize} × {brushSize}</span></div>
      </div>
      <Button className={"min-w-30"} disabled={!matrix} onClick={() => {
        const newMatrix = matrix!.map((row) => row.map(() => paintColor))
        updateAppStore({ matrix: newMatrix })
      }}>Fill canvas</Button>
    </div>
  </>
}


export const ColorPopover: React.FC<{
  paintColor: string,
  setPaintColor: (color: string) => void
}> = ({ paintColor, setPaintColor }) => (
  <Popover className="relative">
    <PopoverButton
      className="p-0.5 border border-black/40 dark:border-white/20 rounded focus:ring-2 ring-blue-300">
      <div
        className="w-10 h-22 rounded-lg border border-white/20 cursor-pointer"
        style={{ backgroundColor: paintColor }}
      />
    </PopoverButton>
    <PopoverPanel
      anchor={{
        "to": "top start",
        "gap": "10px"
      }}
      transition
      className={`${glassPanelClasses} ${popoverTransitionClasses}`}>
      <ColorPicker
        height={250} width={200}
        padding={4} handleRadius={10}
        onChange={(color) => {
          setPaintColor(color.hexString.toUpperCase())
        }}
        layout={[
          { component: iro.ui.Box },
          {
            component: iro.ui.Slider,
            options: { id: "hue-slider", sliderType: "hue" }
          }
        ]}></ColorPicker>
    </PopoverPanel>
  </Popover>
)
