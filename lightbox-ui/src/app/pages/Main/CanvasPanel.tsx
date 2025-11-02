import React, { useEffect } from "react"
import { type LedMatrix, LightboxCanvas } from "../../components/LightboxCanvas.tsx"
import { updateAppStore, useAppStore } from "../../store/appStore.ts"
import { useMutation, useQueryClient, useSuspenseQuery } from "@tanstack/react-query"
import { Button } from "../../components/Button.tsx"
import { api } from "../../lib/api"
import { replaceEqualDeep } from "@tanstack/query-core"
import { ErrorMessage } from "../../components/ErrorMessage.tsx"
import { GenericSuspense } from "../../components/GenericSuspense.tsx"

export const CanvasPanel = () => {
  return (
    <GenericSuspense classNameErrorMessage="text-base!">
      <CanvasContent/>
    </GenericSuspense>
  )
}

function CanvasContent() {
  const { data } = useSuspenseQuery({
    retry: 1,
    staleTime: Infinity,
    queryKey: ["matrix"],
    queryFn: api.getMatrix,
  })

  const { matrix, paintColor, brightness, brushSize } = useAppStore((s) => ({
    matrix: s.matrix ?? data.matrix,
    brightness: s.brightness ?? data.brightness,
    paintColor: s.paintColor,
    brushSize: s.brushSize,
  }))

  useEffect(() => {
    updateAppStore({ matrix: data.matrix, brightness: data.brightness })
  }, [data.matrix, data.brightness])

  const rows = matrix.length
  const columns = matrix[0]?.length || 0

  return (
    <>
      <div className="flex justify-between items-center mb-2">
        <div></div>
        <div className="flex items-center gap-2">
          <span className="text-xs text-gray-500 dark:text-gray-400">
            {columns} × {rows}
          </span>
          {/*<div className="flex items-center gap-2 border border-gray-300 dark:border-gray-600 rounded px-2">*/}
          {/*  <div className="w-2 h-2 bg-green-600 dark:bg-green-400 rounded-full animate-pulse"/>*/}
          {/*  <span className="text-green-600 dark:text-green-400 text-sm">Connected</span>*/}
          {/*</div>*/}
        </div>
      </div>

      <div className="flex justify-center flex-1 w-full" style={{ maxHeight: "calc(100% - 4.5rem)" }}>
        <LightboxCanvas
          matrix={matrix}
          brightness={brightness}
          paintColor={paintColor}
          brushSize={brushSize}
          setMatrix={(m: LedMatrix) => updateAppStore({ matrix: m as string[][] })}
        />
      </div>

      {((data.matrix !== replaceEqualDeep(data.matrix, matrix) || data.brightness != brightness) &&
        <SaveButton matrix={matrix} brightness={brightness}/>)}
    </>
  )
}

function SaveButton({ matrix, brightness }: { matrix: string[][], brightness: number }) {
  const qc = useQueryClient()
  const saveMatrix = useMutation({
    mutationFn: api.postMatrix,
    onSuccess: () => qc.invalidateQueries({ queryKey: ["matrix"] }),
  })
  const [isPending, startTransition] = React.useTransition()

  return (
    <div className="flex justify-end items-center pt-2 gap-3">
      {saveMatrix.isError && (
        <div className="flex items-center gap-1 max-w-[200px] pl-1">
          <ErrorMessage className="line-clamp-2">{saveMatrix.error.message}</ErrorMessage>
        </div>
      )}
      <Button
        loading={saveMatrix.isPending || isPending}
        className="flex items-center gap-2 px-3 py-1 border rounded text-sm disabled:opacity-60"
        onClick={() => {
          startTransition(() => saveMatrix.mutate({ matrix, brightness }))
        }}>
        Save changes
      </Button>
    </div>
  )
}
