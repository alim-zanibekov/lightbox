import React, { type ReactNode } from "react"

export const ErrorMessage: React.FC<{ children: ReactNode, className?: string }> = ({ children, className = "" }) => <p
  className={`border border-dashed border-red-400 backdrop-blur-xl bg-red-500/10 text-red-600 dark:text-red-400 rounded-md p-2 text-xs ${className}`}>
  <span className="line-clamp-2">{children}</span>
</p>
