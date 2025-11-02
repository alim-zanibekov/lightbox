import iro from "@jaames/iro"
import type { ColorPickerProps, IroColorPicker } from "@jaames/iro/dist/ColorPicker"
import { useRef, useEffect } from "react"

interface ColorPickerWrapperProps extends Partial<ColorPickerProps> {
  onChange?: (color: iro.Color) => void;
}

export function ColorPicker({ onChange, ...options }: ColorPickerWrapperProps) {
  const containerRef = useRef<HTMLDivElement>(null)
  const colorPickerRef = useRef<IroColorPicker | null>(null)

  useEffect(() => {
    const container = containerRef.current
    if (!container) return

    const picker = iro.ColorPicker(container, { ...options })
    colorPickerRef.current = picker

    const handleColorChange = (color: iro.Color) => onChange?.(color)
    picker.on("color:change", handleColorChange)

    return () => {
      picker.off("color:change", handleColorChange)
      container.childNodes.forEach((child) => child.remove())
    }
  }, [onChange, options])

  return <div ref={containerRef}></div>
}
