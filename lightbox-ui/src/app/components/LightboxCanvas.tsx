import React, { useCallback, useEffect, useRef, useState } from "react"
import { ListeningCanvas, type ClientPoint } from "./ListeningCanvas"

export type LedMatrix = (string | null)[][]

type GridParams = { gap: number; cellSize: number; rect?: DOMRect }

interface LightboxCanvasProps {
  matrix: LedMatrix,
  setMatrix: (matrix: LedMatrix) => void,
  paintColor: string,
  brushSize?: number,
  brightness?: number,
}

export const LightboxCanvas: React.FC<LightboxCanvasProps> = ({
  matrix,
  setMatrix,
  paintColor,
  brushSize = 1,
  brightness = 100
}) => {
  const containerRef = useRef<HTMLDivElement>(null)
  const [gridParams, setGridParams] = useState<GridParams>({ cellSize: 0, gap: 0 })
  const rows = matrix.length
  const columns = matrix[0]?.length ?? 0

  const onPoint = useCallback(
    (_: ClientPoint, point?: ClientPoint) => {
      if (!point || !gridParams.rect) return

      const [x, y] = point
      const brushPxSize = brushSize * (gridParams.cellSize + gridParams.gap)
      const brushPxHalf = Math.floor(brushPxSize / 2)

      const brushRect = new DOMRect(x - brushPxHalf, y - brushPxHalf, brushPxSize, brushPxSize)
      const newMatrix = matrix.map(r => [...r])

      for (let row = 0; row < rows; row++) {
        for (let col = 0; col < columns; col++) {
          const x = (gridParams.cellSize + gridParams.gap) * col + gridParams.cellSize / 2
          const y = (gridParams.cellSize + gridParams.gap) * row + gridParams.cellSize / 2

          if (
            brushRect.left <= x && x <= brushRect.right &&
            brushRect.top <= y && y <= brushRect.bottom
          ) {
            if (newMatrix[row][col] !== null) {
              newMatrix[row][col] = paintColor
            }
          }
        }
      }

      setMatrix(newMatrix)
    },
    [gridParams.rect, gridParams.cellSize, gridParams.gap, brushSize, matrix, setMatrix, rows, columns, paintColor]
  )

  const aspectRatio = columns / rows

  useEffect(() => {
    const container = containerRef.current
    if (!container) return

    const onResize = () => {
      const rect = container.getBoundingClientRect()
      const styles = window.getComputedStyle(container.querySelector(".grid")!)
      const gap = +styles.getPropertyValue("gap").replace("px", "")
      const cellSize = +styles.getPropertyValue("grid-template-columns").split(" ").pop()!.replace("px", "")
      const zoom = +styles.getPropertyValue("zoom")
      setGridParams({ cellSize: cellSize * zoom, gap: gap * zoom, rect })
    }

    const resizeObserver = new ResizeObserver(onResize)
    resizeObserver.observe(container)
    window.addEventListener("resize", onResize)
    return () => {
      resizeObserver.disconnect()
      window.removeEventListener("resize", onResize)
    }
  }, [columns, rows, containerRef, aspectRatio])

  const maxHeight = "((100dvh - 9rem) * var(--is-lg) + (100dvh - 14rem) * (1 - var(--is-lg)))"

  const minHeight = `max(${maxHeight}, 15rem * ${rows / columns})`
  const minWidth = `max(100dvw - 4rem, 26rem * ${columns / rows})`
  const gap = `((min(${minWidth}, ${minHeight}) / ${Math.max(columns, rows)}) * 0.13)`
  const cellWidth = `((${minWidth} - ${gap} * ${columns})/${columns})`
  const cellHeight = `((${minHeight} - ${gap} * ${rows})/${rows})`
  const cellSize = `min(${cellWidth}, ${cellHeight})`

  const style = {
    gap: `calc(${gap})`,
    gridTemplateRows: `repeat(${matrix.length}, calc(${cellSize}))`,
    gridTemplateColumns: `repeat(${(matrix[0] ?? []).length}, calc(${cellSize}))`,
  }

  return (
    <div ref={containerRef} className="flex w-full max-h-full justify-center">
      <ListeningCanvas onStartStroke={onPoint} onContinueStroke={onPoint}>
        <div
          className="relative grid max-h-fit"
          style={style}
        >
          {matrix.map((items, i) =>
            items.map((it, j) => (
              <div
                key={`led-[${i},${j}]`} data-led-grid-cell={it} data-row={i} data-col={j}
                className={"flex items-center justify-center"}
              >
                {it ? (
                  <div
                    style={{ backgroundColor: it, filter: `brightness(${brightness / 100})` }}
                    className="h-full  aspect-square border border-foreground"
                  />
                ) : null}
              </div>
            ))
          )}
        </div>
      </ListeningCanvas>
    </div>
  )
}
