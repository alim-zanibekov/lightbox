import React, { useCallback, useEffect, useRef, useState } from "react"

export interface SteppedSliderProps {
  min?: number;
  max?: number;
  step?: number;
  value?: number;
  defaultValue?: number;
  onChange?: (value: number) => void;
  className?: string;
  trackClassName?: string;
  trackFilledClassName?: string;
  thumbClassName?: string;
  disabled?: boolean;
}

const clamp = (value: number, min: number, max: number) => Math.min(Math.max(value, min), max)

const roundToStep = (value: number, min: number, stepSize: number) => {
  const n = Math.round((value - min) / stepSize)
  return min + n * stepSize
}

const isNumber = (value: unknown) => typeof value === "number"

export const SteppedSlider: React.FC<SteppedSliderProps> = ({
  min = 0,
  max = 100,
  step,
  value: controlledValue,
  defaultValue,
  onChange,
  className = "",
  trackClassName = "",
  thumbClassName = "",
  trackFilledClassName = "",
  disabled = false,
}) => {
  const trackRef = useRef<HTMLDivElement | null>(null)
  const draggingRef = useRef(false)
  const stepSize = (isNumber(step) && step > 0) ? step : 1

  const [internalValue, setInternalValue] = useState<number>(() => {
    if (isNumber(controlledValue)) return clamp(controlledValue, min, max)
    if (isNumber(defaultValue)) return clamp(defaultValue, min, max)
    return min
  })

  const isControlled = isNumber(controlledValue)

  useEffect(() => {
    if (draggingRef.current) return
    if (isControlled) setInternalValue(clamp(controlledValue, min, max))
  }, [isControlled, controlledValue, min, max])

  const currentValue = isControlled && !draggingRef.current ? controlledValue : internalValue
  const percent = (currentValue - min) / Math.max(1e-9, (max - min))

  const setValueFromPointerEvent = useCallback(
    (clientX: number) => {
      if (!trackRef.current) return

      const rect = trackRef.current.getBoundingClientRect()
      const x = clamp(clientX - rect.left, 0, rect.width)
      const ratio = rect.width === 0 ? 0 : x / rect.width

      const value = min + ratio * (max - min)
      const snapped = (value < max - 1e-9) ? roundToStep(value, min, stepSize) : max

      setInternalValue(snapped)
      onChange?.(snapped)
    },
    [min, max, stepSize, onChange]
  )

  const onPointerMove = useCallback(
    (e: PointerEvent) => {
      if (!draggingRef.current) return
      setValueFromPointerEvent(e.clientX)
    }, [setValueFromPointerEvent]
  )

  const onPointerUp = useCallback(
    () => {
      draggingRef.current = false
      document.removeEventListener("pointermove", onPointerMove)
      document.removeEventListener("pointerup", onPointerUp)
      document.removeEventListener("pointercancel", onPointerUp)
    }, [onPointerMove]
  )

  const addListeners = useCallback(
    () => {
      window.addEventListener("pointermove", onPointerMove)
      window.addEventListener("pointerup", onPointerUp)
      window.addEventListener("pointercancel", onPointerUp)
    }, [onPointerMove, onPointerUp]
  )

  const removeListeners = useCallback(
    () => {
      document.removeEventListener("pointermove", onPointerMove)
      document.removeEventListener("pointerup", onPointerUp)
      document.removeEventListener("pointercancel", onPointerUp)
    }, [onPointerMove, onPointerUp]
  )

  const onTrackPointerDown = useCallback(
    (e: React.PointerEvent<HTMLDivElement>) => {
      if (disabled) return

      e.currentTarget.setPointerCapture(e.pointerId)
      e.preventDefault()
      draggingRef.current = true

      setValueFromPointerEvent(e.clientX)

      addListeners()
    },
    [disabled, setValueFromPointerEvent, addListeners]
  )

  const onThumbPointerDown = useCallback(
    (e: React.PointerEvent<HTMLButtonElement>) => {
      if (disabled) return

      e.currentTarget.setPointerCapture(e.pointerId)
      e.preventDefault()
      draggingRef.current = true

      addListeners()
    },
    [disabled, addListeners]
  )

  useEffect(() => {
    return () => removeListeners()
  }, [removeListeners])

  const thumbLeftPct = `${percent * 100}%`
  return (
    <div className={`w-full h-4 ${className} ${disabled ? "opacity-50 cursor-not-allowed" : ""}`}>
      <div
        ref={trackRef}
        role="presentation"
        className={`relative translate-y-1/2 h-3/5 backdrop-blur-xl bg-black/10 dark:bg-white/10 border border-black/5 rounded-full select-none touch-none ${trackClassName}`}
        onPointerDown={onTrackPointerDown}
        style={{ touchAction: "none", WebkitTouchCallout: "none", userSelect: "none" }}
        onContextMenu={(e) => e.preventDefault()}
      >
        <div
          className={`absolute -left-[1px] -top-[1px] -bottom-[1px] rounded-full bg-gray-700/70 dark:bg-gray-300/70 ${trackFilledClassName}`}
          style={{ width: `${percent * 100}%` }}
        />
        <button
          type="button"
          disabled={disabled}
          onPointerDown={onThumbPointerDown}
          className={`${disabled ? "cursor-not-allowed" : ""} absolute z-10 -translate-x-1/2 -translate-y-1/2 top-1/2 w-4 h-4 rounded-full bg-gray-500 dark:bg-gray-100 border border-white/30 dark:border-black/20 shadow-md focus:outline-none focus:ring-2 focus:ring-offset-1 ${thumbClassName}`}
          onContextMenu={(e) => e.preventDefault()}
          style={{
            left: thumbLeftPct, touchAction: "none", WebkitTouchCallout: "none", userSelect: "none"
          }}
        >
        </button>
      </div>
    </div>
  )
}
