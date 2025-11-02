import { useRef, useCallback, useState } from "react"
import type { PropsWithChildren, PointerEvent } from "react"

export type ClientPoint = [number, number]

const getPoint = (event: PointerEvent): ClientPoint => [event.clientX, event.clientY]
const getRelativePoint = (event: PointerEvent, parent: HTMLDivElement | null): ClientPoint | undefined => {
  const rect = parent?.getBoundingClientRect()
  if (!rect) return undefined
  return [event.clientX - rect.left, event.clientY - rect.top]
}

export function ListeningCanvas({
  children,
  onStartStroke,
  onContinueStroke,
  onEndStroke,
}: PropsWithChildren & {
  onStartStroke?: (p: ClientPoint, relativePoint?: ClientPoint) => void
  onContinueStroke?: (p: ClientPoint, relativePoint?: ClientPoint) => void
  onEndStroke?: () => void
}) {
  const ref = useRef<HTMLDivElement>(null)
  const [drawing, setDrawing] = useState(false)

  const startStroke = useCallback(
    (event: PointerEvent) => {
      if (drawing) return
      event.preventDefault()
      setDrawing(true)
      onStartStroke?.(getPoint(event), getRelativePoint(event, ref.current))
    },
    [drawing, onStartStroke]
  )

  const continueStroke = useCallback(
    (event: PointerEvent) => {
      if (!drawing) return
      onContinueStroke?.(getPoint(event), getRelativePoint(event, ref.current))
    },
    [drawing, onContinueStroke]
  )

  const endStroke = useCallback(() => {
    if (!drawing) return
    setDrawing(false)
    onEndStroke?.()
  }, [drawing, onEndStroke])

  return (
    <div
      ref={ref}
      onPointerDown={startStroke}
      onPointerMove={continueStroke}
      onPointerUp={endStroke}
      onPointerCancel={endStroke}
      onPointerLeave={(e) => {
        if (drawing) continueStroke(e)
      }}
      onPointerEnter={(e) => {
        if (drawing && e.buttons) startStroke(e)
        else if (drawing) endStroke()
      }}
      onContextMenu={(e) => e.preventDefault()}
      style={{ touchAction: "none", WebkitTouchCallout: "none", userSelect: "none" }}
    >
      {children}
    </div>
  )
}
