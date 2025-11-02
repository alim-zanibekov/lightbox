import React, { type ReactNode } from "react"
import { Loader2 } from "lucide-react"

export interface ButtonProps extends React.ButtonHTMLAttributes<HTMLButtonElement> {
  variant?: "primary" | "danger" | "ghost";
  size?: "xs" | "sm" | "md" | "lg";
  loading?: boolean;
  children: ReactNode;
}

export const Button: React.FC<ButtonProps> = ({
  variant = "primary",
  size = "md",
  loading = false,
  disabled,
  children,
  className = "",
  ...props
}) => {
  const baseClasses = "inline-flex gap-1 items-center justify-center font-medium rounded-lg " +
    "transition-colors focus:outline-none focus:ring-2 ring-blue-500/50 focus:ring-offset-1 ring-offset-blue-400 " +
    "disabled:opacity-50 disabled:cursor-not-allowed cursor-pointer"

  const variantClasses = {
    primary: "backdrop-blur-xl bg-black/10 border border-black/5 dark:bg-white/10 dark:border-white/20 text-gray-700 dark:text-white active:bg-black/20 active:dark:bg-white/20",
    danger: "backdrop-blur-xl bg-red-500/10 border border-red-500/5 dark:border-white/20 text-red-600 dark:text-red-400 active:bg-red-500/20",
    ghost: "text-gray-700 dark:text-gray-100 hover:bg-black/10 focus:ring-gray-500 ring-offset-gray-300"
  }

  const sizeClasses = {
    xs: "px-1.5 py-1 text-xs",
    sm: "px-2 py-1.5 text-sm",
    md: "px-4 py-2 text-sm",
    lg: "px-6 py-3 text-base"
  }

  return (
    <button
      disabled={disabled || loading}
      className={`${baseClasses} ${variantClasses[variant]} ${sizeClasses[size]} ${className}`}
      {...props}
    >
      {loading && (
        <Loader2 className="animate-spin -ml-1 mr-2 h-4 w-4"/>
      )}
      {children}
    </button>
  )
}
