import { QueryErrorResetBoundary } from "@tanstack/react-query"
import { ErrorBoundary } from "react-error-boundary"
import { Button } from "./Button.tsx"
import React, { type ReactNode } from "react"
import { ErrorMessage } from "./ErrorMessage.tsx"
import { RotateCcw } from "lucide-react"

export const GenericSuspense: React.FC<{
  children: ReactNode
  classNameErrorMessage?: string
  classNameErrorButton?: string
}> = ({ children, classNameErrorMessage = "", classNameErrorButton = "" }) => {
  return (
    <QueryErrorResetBoundary>
      {({ reset }) => (
        <ErrorBoundary
          fallbackRender={({ error, resetErrorBoundary }) => (
            <div className="flex flex-col items-center justify-center h-full gap-2">
              <ErrorMessage className={`${classNameErrorMessage} line-clamp-4`}>{error.message}</ErrorMessage>
              <Button size="sm" onClick={() => resetErrorBoundary()} className={classNameErrorButton}>
                <RotateCcw size={14} className="mr-1"/>
                Try again
              </Button>
            </div>
          )}
          onReset={reset}
        >
          <React.Suspense
            fallback={
              <div className="flex items-center justify-center h-full pb-2">
                <div className="w-6 h-6 border-2 border-t-transparent border-gray-400 rounded-full animate-spin"/>
              </div>
            }
          >
            {children}
          </React.Suspense>
        </ErrorBoundary>
      )}
    </QueryErrorResetBoundary>
  )
}
